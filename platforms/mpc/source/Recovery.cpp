// MPDJ (2026-11-16): workspace recovery, see Recovery.h
#include "Recovery.h"
#include "mpdjFeatures.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

namespace
{
std::string root()
{
    const char* r = getenv("MPDJ_ROOT");
    std::string s = r && *r ? r : "/media/MPC SDC/MPDJ";
    while (s.size() > 1 && s.back() == '/')
        s.pop_back();
    return s;
}

bool isDir(const std::string& p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool exists(const std::string& p) { return access(p.c_str(), F_OK) == 0; }

long fileSize(const std::string& p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0 ? (long)st.st_size : -1;
}

bool copyFile(const std::string& from, const std::string& to)
{
    FILE* a = fopen(from.c_str(), "rb");
    if (!a)
        return false;
    FILE* b = fopen(to.c_str(), "wb");
    if (!b)
    {
        fclose(a);
        return false;
    }
    static char buf[1 << 16];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof buf, a)) > 0)
        ok = ok && fwrite(buf, 1, n, b) == n;
    fclose(a);
    ok = (fclose(b) == 0) && ok;
    return ok;
}

// mirror one folder's files (not sub-folders): copy new / changed (by size, or always
// when `always`), remove what the source lacks. Returns the number copied, -1 on error
int mirror(const std::string& from, const std::string& to, bool always)
{
    if (!isDir(from))
        return 0;
    mkdir(to.c_str(), 0755);
    int copied = 0;
    if (DIR* d = opendir(from.c_str()))
    {
        while (dirent* e = readdir(d))
        {
            if (e->d_name[0] == '.')
                continue;
            std::string a = from + "/" + e->d_name, b = to + "/" + e->d_name;
            if (isDir(a))
                continue;
            if (always || fileSize(a) != fileSize(b))
            {
                if (!copyFile(a, b))
                {
                    closedir(d);
                    return -1;
                }
                copied++;
            }
        }
        closedir(d);
    }
    if (DIR* d = opendir(to.c_str()))
    {
        while (dirent* e = readdir(d))
            if (e->d_name[0] != '.' && !exists(from + "/" + e->d_name) && !isDir(to + "/" + e->d_name))
                remove((to + "/" + e->d_name).c_str());
        closedir(d);
    }
    return copied;
}

int mirrorWorkspace(const std::string& from, const std::string& to, bool always)
{
    mkdir(to.c_str(), 0755);
    int a = mirror(from, to, always); // project.mt and the rest at the top
    int b = mirror(from + "/patterns", to + "/patterns", always);
    int c = mirror(from + "/instruments", to + "/instruments", always);
    int s = mirror(from + "/samples", to + "/samples", always);
    if (a < 0 || b < 0 || c < 0 || s < 0)
        return -1;
    return a + b + c + s;
}
} // namespace

namespace Recovery
{
void atLaunch()
{
    if (!mpdjOn(MpdjFeature::Recovery))
        return;
    std::string ws = root() + "/Workspace", snap = root() + "/Workspace.restore";
    std::string running = snap + "/.running", restore = snap + "/RESTORE";
    if (exists(restore) && isDir(ws))
    {
        int n = mirrorWorkspace(snap, ws, true);
        remove(restore.c_str());
        printf("recovery: workspace restored from Workspace.restore (%d files)%s\n", n,
               n < 0 ? " - FAILED part way" : "");
    }
    else if (exists(running))
        printf("recovery: the last session didn't end cleanly; the snapshot from before it is kept "
               "(File > Restore brings it back)\n");
    else if (isDir(ws))
    {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        int n = mirrorWorkspace(ws, snap, false);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("recovery: launch snapshot in Workspace.restore (%d files changed, %.1f s)\n", n,
               (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9);
    }
    if (isDir(snap))
        if (FILE* f = fopen(running.c_str(), "wb"))
            fclose(f);
}

void atCleanExit()
{
    remove((root() + "/Workspace.restore/.running").c_str());
}
}
