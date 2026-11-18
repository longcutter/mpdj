// MPDJ (2026-11-16): the System menu, see SystemMenu.h
#include "SystemMenu.h"

#include "core/interfacePopups.h"
#include "fileManager.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern volatile bool g_running;

namespace SystemMenu
{
namespace
{
enum Item
{
    kRestart,
    kSwitch,
    kStartUp,
    kPowerOff,
    kClose,
    kItems
};
const char* const kNames[kItems] = { "Restart MPDJ", "Switch to the MPC app", "Start up in", "Power off", "Close" };

bool s_open = false;
int s_item = 0;
int s_armed = -1;       // Switch / Power off: pushed once, waiting for the second push
uint32_t s_armedMs = 0; // when (a second push within 3 s does it)
uint32_t s_shownMs = 0, s_now = 0;
Exit s_exit = Exit::None;

char s_exe[256] = "";   // the running binary
char s_dir[256] = ".";  // its folder
char** s_argv = nullptr;
char s_line[4][64];

const char* kServiceFile = "/etc/systemd/system/mpdj.service";

void path(char* out, size_t n, const char* name) { snprintf(out, n, "%s/%s", s_dir, name); }

bool exists(const char* p)
{
    struct stat st;
    return stat(p, &st) == 0;
}

bool newBuildWaiting()
{
    char p[300];
    path(p, sizeof p, "tracker-mpc.new");
    return exists(p);
}

void show()
{
    snprintf(s_line[0], sizeof s_line[0], "System  (%d of %d)", s_item + 1, (int)kItems);
    if (s_item == kStartUp)
        snprintf(s_line[1], sizeof s_line[1], "< Start up in: %s >", bootIntoMpdj() ? "MPDJ" : "MPC app");
    else
        snprintf(s_line[1], sizeof s_line[1], "< %s >", kNames[s_item]);
    const char* detail = "";
    switch (s_item)
    {
        case kRestart:
            detail = newBuildWaiting() ? "Saves, then starts the new build (tracker-mpc.new)"
                                       : "Saves, then starts MPDJ again";
            break;
        case kSwitch: detail = "Saves, quits MPDJ, starts the MPC app"; break;
        case kStartUp:
            detail = exists(kServiceFile) ? "What the power button starts (push: change)"
                                          : "Push: change (needs the startup service)";
            break;
        case kPowerOff: detail = "Saves, then turns the MPC off"; break;
        default: detail = "Closes this menu"; break;
    }
    snprintf(s_line[2], sizeof s_line[2], "%s", detail);
    if (s_armed == s_item)
        snprintf(s_line[3], sizeof s_line[3], "Push %s to %s", s_item == kSwitch ? "again" : "(or power) again",
                 s_item == kSwitch ? "switch" : "power off");
    else
        snprintf(s_line[3], sizeof s_line[3], "Encoder: choose   Push: do   Button: close");
    mtPopups.show(0, s_line[0], s_line[1], s_line[2], s_line[3], 60);
    s_shownMs = s_now;
}

void setBoot(bool mpdj)
{
    char p[300];
    path(p, sizeof p, "boot-mode");
    if (FILE* f = fopen(p, "w"))
    {
        fputs(mpdj ? "mpdj\n" : "mpc\n", f);
        fclose(f);
        sync();
        printf("system: start up in %s (%s)\n", mpdj ? "MPDJ" : "the MPC app", p);
    }
    else
        printf("system: can't write %s: %s\n", p, strerror(errno));
}

void leave(Exit e)
{
    s_exit = e;
    mtPopups.show(0, e == Exit::Restart ? "Restarting MPDJ..." : e == Exit::PowerOff ? "Powering off..." : "Starting the MPC app...");
    s_open = false;
    g_running = false;
}
} // namespace

void setExecutable(const char* exe, char** argv)
{
    snprintf(s_exe, sizeof s_exe, "%s", exe);
    snprintf(s_dir, sizeof s_dir, "%s", exe);
    if (char* slash = strrchr(s_dir, '/'))
        *slash = 0;
    else
        snprintf(s_dir, sizeof s_dir, ".");
    s_argv = argv;
}

bool isOpen() { return s_open; }

void open()
{
    s_open = true;
    s_item = 0;
    s_armed = -1;
    show();
}

void powerButton()
{
    if (s_open && s_item == kPowerOff && s_armed == kPowerOff)
    {
        leave(Exit::PowerOff); // pressed again
        return;
    }
    s_open = true;
    s_item = kPowerOff;
    s_armed = kPowerOff;
    s_armedMs = s_now;
    show();
}

void close()
{
    if (!s_open)
        return;
    s_open = false;
    s_armed = -1;
    mtPopups.hide();
}

void turn(int dir)
{
    if (!s_open)
        return;
    s_item = (s_item + (dir > 0 ? 1 : -1) + kItems) % kItems;
    s_armed = -1;
    show();
}

void push()
{
    if (!s_open)
        return;
    switch (s_item)
    {
        case kRestart: leave(Exit::Restart); return;
        case kStartUp:
            setBoot(!bootIntoMpdj());
            show();
            return;
        case kSwitch:
        case kPowerOff:
            if (s_armed == s_item && s_now - s_armedMs < 5000)
            {
                leave(s_item == kSwitch ? Exit::SwitchToMpc : Exit::PowerOff);
                return;
            }
            s_armed = s_item;
            s_armedMs = s_now;
            show();
            return;
        default: close(); return;
    }
}

void poll(uint32_t nowMs)
{
    s_now = nowMs;
    // up for 30 s: this start counts as good for the startup service (it falls back to the
    // MPC app after 3 starts in a row that didn't get this far)
    static uint32_t s_firstMs = nowMs;
    static bool s_counted = false;
    if (!s_counted && nowMs - s_firstMs >= 30000)
    {
        s_counted = true;
        char p[300];
        path(p, sizeof p, "boot-fails");
        if (exists(p))
            if (FILE* f = fopen(p, "w"))
            {
                fputs("0\n", f);
                fclose(f);
            }
    }
    if (!s_open)
        return;
    if (s_armed >= 0 && nowMs - s_armedMs >= 5000)
    {
        s_armed = -1;
        show();
    }
    else if (nowMs - s_shownMs > 50000)
        show(); // keep it up
}

Exit requested() { return s_exit; }

bool bootIntoMpdj()
{
    char p[300];
    path(p, sizeof p, "boot-mode");
    FILE* f = fopen(p, "r");
    if (!f)
        return true; // the startup service's default: MPDJ
    char buf[16] = "";
    fgets(buf, sizeof buf, f);
    fclose(f);
    return strncmp(buf, "mpc", 3) != 0;
}

int finish()
{
    switch (s_exit)
    {
        case Exit::Restart:
        {
            // a new build copied in as tracker-mpc.new (a running binary can't be written over)
            char fresh[300];
            path(fresh, sizeof fresh, "tracker-mpc.new");
            if (exists(fresh))
            {
                chmod(fresh, 0755);
                if (rename(fresh, s_exe) == 0)
                    printf("system: new build in place (%s)\n", s_exe);
                else
                    printf("system: can't move %s over %s: %s\n", fresh, s_exe, strerror(errno));
            }
            printf("system: restarting %s\n", s_exe);
            fflush(stdout);
            for (int fd = 3; fd < 1024; fd++) // devices the new MPDJ has to open again
                ::close(fd);
            if (s_argv)
                execv(s_exe, s_argv);
            printf("system: restart failed: %s\n", strerror(errno));
            return 1;
        }
        case Exit::SwitchToMpc:
            printf("system: starting the MPC app\n");
            fflush(stdout);
            return system("systemctl --no-block start inmusic-mpc") == 0 ? 0 : 1;
        case Exit::PowerOff:
            printf("system: power off\n");
            fflush(stdout);
            sync();
            return system("systemctl --no-block poweroff") == 0 ? 0 : 1;
        default: return 0;
    }
}
} // namespace SystemMenu
