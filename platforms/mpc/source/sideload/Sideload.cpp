#include "Sideload.h"

#include "PlatformImpl.h"
#include "audio/MpcAudio.h"
#include "mtSequencer.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace
{
const char* command = nullptr;
bool requested = false;
int cyclesLeft = 0, cyclesTotal = 0, runs = 0;
uint32_t nextCycleMs = 0, gapMs = 5000;

constexpr uint8_t kNoteShift = 49, kNoteMain = 52;

uint64_t nowMs()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + ts.tv_nsec / 1000000;
}

int envInt(const char* name, int def)
{
    const char* v = getenv(name);
    return v && *v ? atoi(v) : def;
}

// Watches the controller bytes for SHIFT and MAIN (channel 1 notes) while the
// sideload runs; the bytes themselves are passed on unchanged.
struct ComboWatch
{
    uint8_t status = 0, d[2] = {}, n = 0;
    bool shift = false, main = false;
    void feed(uint8_t b)
    {
        if (b >= 0xF8)
            return;
        if (b & 0x80)
        {
            status = b < 0xF0 ? b : 0;
            n = 0;
            return;
        }
        if (!status)
            return;
        d[n++] = b;
        uint8_t type = status & 0xF0;
        int need = (type == 0xC0 || type == 0xD0) ? 1 : 2;
        if (n < need)
            return;
        n = 0;
        if ((status & 0x0F) != 0 || (type != 0x90 && type != 0x80))
            return;
        bool on = type == 0x90 && d[1] > 0;
        if (d[0] == kNoteShift)
            shift = on;
        else if (d[0] == kNoteMain)
            main = on;
    }
};

// Live (non-zombie) processes in process group pgid, from /proc/<pid>/stat.
int groupAlive(pid_t pgid)
{
    DIR* d = opendir("/proc");
    if (!d)
        return kill(-pgid, 0) == 0 ? 1 : 0;
    int alive = 0;
    while (dirent* e = readdir(d))
    {
        if (e->d_name[0] < '1' || e->d_name[0] > '9')
            continue;
        char path[64], buf[512];
        snprintf(path, sizeof path, "/proc/%s/stat", e->d_name);
        int fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0)
            continue;
        ssize_t n = read(fd, buf, sizeof buf - 1);
        close(fd);
        if (n <= 0)
            continue;
        buf[n] = 0;
        // pid (comm) state ppid pgrp ...; comm may contain spaces, so start after the last ')'
        const char* p = strrchr(buf, ')');
        char state = 0;
        int ppid = 0, pgrp = 0;
        if (p && sscanf(p + 1, " %c %d %d", &state, &ppid, &pgrp) == 3 && pgrp == pgid && state != 'Z' && state != 'X')
            alive++;
    }
    closedir(d);
    return alive;
}

void run()
{
    runs++;
    printf("sideload: #%d handing over to: %s\n", runs, command);
    uint64_t t0 = nowMs();

    if (sequencer.isPlay())
        sequencer.stop();
    platform.input.suspend();
    MpcAudio::suspend();
    platform.display.suspend();

    int toChild[2], fromChild[2];
    if (pipe2(toChild, O_CLOEXEC) != 0 || pipe2(fromChild, O_CLOEXEC) != 0)
    {
        printf("sideload: pipe failed: %s\n", strerror(errno));
        return;
    }
    fcntl(toChild[1], F_SETFL, O_NONBLOCK);   // a sideload that doesn't read MIDI never blocks us
    fcntl(fromChild[0], F_SETFL, O_NONBLOCK);

    prctl(PR_SET_CHILD_SUBREAPER, 1); // orphans of the sideload come back to us to reap
    pid_t pid = fork();
    if (pid == 0)
    {
        setpgid(0, 0); // own process group: SHIFT+MAIN stops everything it started
        dup2(toChild[0], 3);
        dup2(fromChild[1], 4);
        for (int fd = 5; fd < 1024; fd++)
            close(fd);
        setenv("MPDJ_MIDI_IN_FD", "3", 1);
        setenv("MPDJ_MIDI_OUT_FD", "4", 1);
        signal(SIGPIPE, SIG_DFL);
        execl("/bin/sh", "sh", "-c", command, (char*)nullptr);
        _exit(127);
    }
    if (pid > 0)
        setpgid(pid, pid); // also here, so the group exists before we wait on it
    close(toChild[0]);
    close(fromChild[1]);

    int ctl = platform.input.controllerFd();
    ComboWatch combo;
    uint64_t comboSince = 0, termSentAt = 0;
    bool killed = false, done = false, killSent = false, childDone = false;
    uint64_t childDoneAt = 0, lastGroupCheck = 0;
    int status = 0;
    uint64_t relayedIn = 0, relayedOut = 0;

    if (pid < 0)
    {
        printf("sideload: fork failed: %s\n", strerror(errno));
        done = true;
    }
    while (!done)
    {
        pollfd p[2] = { { ctl, POLLIN, 0 }, { fromChild[0], POLLIN, 0 } };
        poll(p, 2, 20);

        uint8_t buf[256];
        if (ctl >= 0)
            for (;;)
            {
                ssize_t n = read(ctl, buf, sizeof buf);
                if (n <= 0)
                    break;
                for (ssize_t i = 0; i < n; i++)
                    combo.feed(buf[i]);
                if (write(toChild[1], buf, (size_t)n) == n)
                    relayedIn += (uint64_t)n;
            }
        for (;;)
        {
            ssize_t n = read(fromChild[0], buf, sizeof buf);
            if (n <= 0)
                break;
            if (ctl >= 0 && write(ctl, buf, (size_t)n) == n)
                relayedOut += (uint64_t)n;
        }

        uint64_t now = nowMs();
        if (combo.shift && combo.main)
        {
            if (!comboSince)
                comboSince = now;
        }
        else
            comboSince = 0;

        bool stopIt = (comboSince && now - comboSince >= Sideload::kComboHoldMs) || !g_running;
        if (stopIt && !termSentAt)
        {
            printf("sideload: %s, stopping it\n", g_running ? "SHIFT+MAIN held" : "MPDJ quitting");
            kill(-pid, SIGTERM);
            termSentAt = now;
            killed = true;
        }
        if (termSentAt && !killSent && now - termSentAt >= 1500)
        {
            printf("sideload: still running 1.5 s after SIGTERM, sending SIGKILL\n");
            kill(-pid, SIGKILL);
            killSent = true;
        }

        // Reap the sideload and anything in its group (orphans come to us as
        // the subreaper). It is over when the whole group is gone.
        for (;;)
        {
            int st;
            pid_t r = waitpid(-pid, &st, WNOHANG);
            if (r > 0)
            {
                if (r == pid)
                {
                    childDone = true;
                    status = st;
                    childDoneAt = now;
                }
                continue;
            }
            break;
        }
        // Over when nothing in its group is left alive. Orphans may have gone
        // to init rather than to us (qemu-user), and a zombie nobody reaps
        // still counts for kill(-pid, 0), so look at /proc instead.
        if (now - lastGroupCheck >= 100)
        {
            lastGroupCheck = now;
            if (groupAlive(pid) == 0)
            {
                while (waitpid(-pid, nullptr, WNOHANG) > 0)
                    ;
                done = true;
            }
        }
        if (!done && childDone && !termSentAt && now - childDoneAt >= 1000)
        {
            printf("sideload: it exited but left processes running, stopping them\n");
            kill(-pid, SIGTERM);
            termSentAt = now;
        }
    }
    close(toChild[1]);
    close(fromChild[0]);

    double secs = (nowMs() - t0) / 1000.0;
    if (pid <= 0)
        ;
    else if (!childDone)
        printf("sideload: #%d gone after %.1f s\n", runs, secs);
    else if (WIFEXITED(status))
        printf("sideload: #%d exited with status %d after %.1f s%s\n", runs, WEXITSTATUS(status), secs,
               killed ? " (stopped by MPDJ)" : "");
    else if (WIFSIGNALED(status))
        printf("sideload: #%d ended by signal %d after %.1f s%s\n", runs, WTERMSIG(status), secs,
               killed ? " (stopped by MPDJ)" : "");
    printf("sideload: relayed %llu MIDI bytes to it, %llu back to the controller\n",
           (unsigned long long)relayedIn, (unsigned long long)relayedOut);

    uint64_t t1 = nowMs();
    MpcAudio::resume();
    platform.input.resume();
    platform.display.resume();
    printf("sideload: #%d back in MPDJ (took %llu ms to take everything back)\n", runs,
           (unsigned long long)(nowMs() - t1));
}
} // namespace

void Sideload::init()
{
    const char* c = getenv("MPDJ_SIDELOAD");
    command = c && *c ? c : nullptr;
    if (!command)
        return;
    cyclesTotal = cyclesLeft = envInt("MPDJ_SIDELOAD_CYCLE", 0);
    gapMs = (uint32_t)envInt("MPDJ_SIDELOAD_GAP", 5) * 1000u;
    nextCycleMs = platform.getTimer()->millis() + (uint32_t)envInt("MPDJ_SIDELOAD_START", 15) * 1000u;
    printf("sideload: %s (hold SHIFT+MAIN 2 s to hand over and to come back)\n", command);
    if (cyclesLeft > 0)
        printf("sideload: automatic test, %d hand-overs, first in %d s\n", cyclesLeft,
               envInt("MPDJ_SIDELOAD_START", 15));
}

bool Sideload::enabled()
{
    return command != nullptr;
}

void Sideload::request()
{
    if (command)
        requested = true;
}

void Sideload::poll()
{
    if (!command)
        return;
    bool cycleDue = cyclesLeft > 0 && (int32_t)(platform.getTimer()->millis() - nextCycleMs) >= 0;
    if (!requested && !cycleDue)
        return;
    requested = false;
    run();
    if (cycleDue)
    {
        cyclesLeft--;
        nextCycleMs = platform.getTimer()->millis() + gapMs;
        if (cyclesLeft == 0)
            printf("sideload: automatic test done, %d hand-overs\n", cyclesTotal);
    }
}
