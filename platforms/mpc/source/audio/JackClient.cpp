#include "JackClient.h"

#include <atomic>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

// The few JACK declarations we need, written out so no JACK headers are needed.
namespace
{
struct jack_client_t;
struct jack_port_t;
typedef uint32_t jack_nframes_t;
struct jack_latency_range_t
{
    jack_nframes_t min, max;
};
enum : unsigned long
{
    JackPortIsInput = 0x1,
    JackPortIsOutput = 0x2,
    JackPortIsPhysical = 0x4,
};
enum
{
    JackNoStartServer = 0x01,
};
enum
{
    JackCaptureLatency = 0,
    JackPlaybackLatency = 1,
};
const char* const kAudioType = "32 bit float mono audio";

struct Api
{
    jack_client_t* (*client_open)(const char*, int, int*, ...);
    int (*client_close)(jack_client_t*);
    jack_port_t* (*port_register)(jack_client_t*, const char*, const char*, unsigned long, unsigned long);
    void* (*port_get_buffer)(jack_port_t*, jack_nframes_t);
    const char* (*port_name)(const jack_port_t*);
    int (*set_process_callback)(jack_client_t*, int (*)(jack_nframes_t, void*), void*);
    int (*set_xrun_callback)(jack_client_t*, int (*)(void*), void*);
    void (*on_shutdown)(jack_client_t*, void (*)(void*), void*);
    int (*activate)(jack_client_t*);
    int (*deactivate)(jack_client_t*);
    int (*connect)(jack_client_t*, const char*, const char*);
    const char** (*get_ports)(jack_client_t*, const char*, const char*, unsigned long);
    void (*free_)(void*);
    jack_nframes_t (*get_sample_rate)(jack_client_t*);
    jack_nframes_t (*get_buffer_size)(jack_client_t*);
    float (*cpu_load)(jack_client_t*);
    jack_port_t* (*port_by_name)(jack_client_t*, const char*);
    void (*port_get_latency_range)(jack_port_t*, int, jack_latency_range_t*);
} api;

void* lib = nullptr;
jack_client_t* client = nullptr;
jack_port_t *inPort[2], *auxPort[2], *outPort[2];
JackClient::Process processFn = nullptr;
void* processUser = nullptr;
bool active = false;
std::atomic<unsigned> xruns{ 0 };
std::atomic<bool> gone{ false };

template <typename F> bool sym(F& f, const char* name)
{
    f = reinterpret_cast<F>(dlsym(lib, name));
    if (!f)
        printf("jack: libjack has no %s\n", name);
    return f != nullptr;
}

bool loadLib()
{
    if (lib)
        return true;
    const char* path = "libjack.so.0";
    lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!lib)
    {
        printf("jack: can't load %s (%s). Run MPDJ inside the Debian root with jackd2 installed "
               "(tools/sideload/s3a/s3a-run.sh)\n",
               path, dlerror());
        return false;
    }
    bool ok = sym(api.client_open, "jack_client_open") && sym(api.client_close, "jack_client_close") &&
              sym(api.port_register, "jack_port_register") && sym(api.port_get_buffer, "jack_port_get_buffer") &&
              sym(api.port_name, "jack_port_name") && sym(api.set_process_callback, "jack_set_process_callback") &&
              sym(api.set_xrun_callback, "jack_set_xrun_callback") && sym(api.on_shutdown, "jack_on_shutdown") &&
              sym(api.activate, "jack_activate") && sym(api.deactivate, "jack_deactivate") &&
              sym(api.connect, "jack_connect") && sym(api.get_ports, "jack_get_ports") &&
              sym(api.free_, "jack_free") && sym(api.get_sample_rate, "jack_get_sample_rate") &&
              sym(api.get_buffer_size, "jack_get_buffer_size") && sym(api.cpu_load, "jack_cpu_load") &&
              sym(api.port_by_name, "jack_port_by_name") &&
              sym(api.port_get_latency_range, "jack_port_get_latency_range");
    if (!ok)
    {
        dlclose(lib);
        lib = nullptr;
    }
    return ok;
}

int onProcess(jack_nframes_t n, void*)
{
    float* b[6];
    for (int c = 0; c < 2; c++)
    {
        b[c] = (float*)api.port_get_buffer(inPort[c], n);
        b[2 + c] = (float*)api.port_get_buffer(auxPort[c], n);
        b[4 + c] = (float*)api.port_get_buffer(outPort[c], n);
    }
    if (processFn)
        processFn(b[0], b[1], b[2], b[3], b[4], b[5], n, processUser);
    else
    {
        memset(b[4], 0, n * sizeof(float));
        memset(b[5], 0, n * sizeof(float));
    }
    return 0;
}

int onXrun(void*)
{
    xruns.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

void onShutdown(void*)
{
    gone = true;
}

// Connects our ports to the first two physical ports of the other direction.
void connectSystem(bool inputs)
{
    if (const char** p = api.get_ports(client, nullptr, kAudioType, JackPortIsPhysical | JackPortIsInput))
    {
        for (int c = 0; c < 2 && p[c]; c++)
            if (api.connect(client, api.port_name(outPort[c]), p[c]) != 0)
                printf("jack: can't connect %s -> %s\n", api.port_name(outPort[c]), p[c]);
        api.free_(p);
    }
    if (!inputs)
        return;
    if (const char** p = api.get_ports(client, nullptr, kAudioType, JackPortIsPhysical | JackPortIsOutput))
    {
        for (int c = 0; c < 2 && p[c]; c++)
            if (api.connect(client, p[c], api.port_name(inPort[c])) != 0)
                printf("jack: can't connect %s -> %s\n", p[c], api.port_name(inPort[c]));
        api.free_(p);
    }
}
} // namespace

bool JackClient::open(const char* name, Process process, void* user)
{
    if (client)
        return true;
    if (!loadLib())
        return false;
    int status = 0;
    client = api.client_open(name, JackNoStartServer, &status);
    if (!client)
    {
        printf("jack: can't connect to jackd (status 0x%x). Is jackd running in the Debian root?\n", status);
        return false;
    }
    processFn = process;
    processUser = user;
    gone = false;
    const char* names[3][2] = { { "in_1", "in_2" }, { "aux_1", "aux_2" }, { "out_1", "out_2" } };
    for (int c = 0; c < 2; c++)
    {
        inPort[c] = api.port_register(client, names[0][c], kAudioType, JackPortIsInput, 0);
        auxPort[c] = api.port_register(client, names[1][c], kAudioType, JackPortIsInput, 0);
        outPort[c] = api.port_register(client, names[2][c], kAudioType, JackPortIsOutput, 0);
        if (!inPort[c] || !auxPort[c] || !outPort[c])
        {
            printf("jack: port registration failed\n");
            close();
            return false;
        }
    }
    api.set_process_callback(client, onProcess, nullptr);
    api.set_xrun_callback(client, onXrun, nullptr);
    api.on_shutdown(client, onShutdown, nullptr);
    return true;
}

bool JackClient::activate(bool connectInputs)
{
    if (!client)
        return false;
    if (active)
        return true;
    if (api.activate(client) != 0)
    {
        printf("jack: activate failed\n");
        return false;
    }
    active = true;
    connectSystem(connectInputs);
    return true;
}

void JackClient::deactivate()
{
    if (client && active && !gone)
        api.deactivate(client);
    active = false;
}

void JackClient::close()
{
    deactivate();
    if (client && !gone)
        api.client_close(client);
    client = nullptr;
}

bool JackClient::isOpen()
{
    return client != nullptr;
}

unsigned JackClient::sampleRate()
{
    return client ? api.get_sample_rate(client) : 0;
}

unsigned JackClient::bufferSize()
{
    return client ? api.get_buffer_size(client) : 0;
}

float JackClient::cpuLoad()
{
    return client && !gone ? api.cpu_load(client) : 0.0f;
}

unsigned JackClient::takeXruns()
{
    return xruns.exchange(0);
}

bool JackClient::serverGone()
{
    return gone;
}

unsigned JackClient::systemLatency(bool capture)
{
    if (!client)
        return 0;
    const char** p = api.get_ports(client, nullptr, kAudioType,
                                   JackPortIsPhysical | (capture ? JackPortIsOutput : JackPortIsInput));
    if (!p)
        return 0;
    jack_latency_range_t r = { 0, 0 };
    if (p[0])
        if (jack_port_t* port = api.port_by_name(client, p[0]))
            api.port_get_latency_range(port, capture ? JackCaptureLatency : JackPlaybackLatency, &r);
    api.free_(p);
    return r.max;
}
