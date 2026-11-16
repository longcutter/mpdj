// MPDJ additions (MIT, see NOTICE.md): Kits (see mpdjKits.h)
#include "mpdjKits.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "Platform.h"
#include "fileManager.h"
#include "fileManagerDefs.h"
#include "fileTransfer.h"
#include "mpdjPads.h"
#include "mtSequencer.h"
#include "mtStructs.h"

namespace mpdjKits
{
static FileSystem* fs() { return newFileManager.getFileSystem(); }

bool isKitFolder(const char* folderPath)
{
    char path[256];
    snprintf(path, sizeof path, "%s/%s", folderPath, kInfoFile);
    return fs()->exists(path);
}

bool exists(const char* name)
{
    char path[256];
    snprintf(path, sizeof path, "%s/%s", kFolder, name);
    return fs()->exists(path);
}

int lastPlayedBank()
{
    int p = mpdjLastPadIndex();
    return p >= 0 ? p / kPadsPerBank : 0;
}

static const char* const kModeWord[kPadModeCount] = { "auto", "oneshot", "gate" };

static bool readText(const char* path, char* buf, int size)
{
    auto [ok, h] = fs()->openFile(path, FileSystem::kRead);
    if (!ok)
        return false;
    int32_t n = fs()->readFile(buf, size - 1, h);
    fs()->closeFile(h);
    if (n < 0)
        return false;
    buf[n] = 0;
    return true;
}

static bool writeText(const char* path, const char* text)
{
    if (fs()->exists(path))
        fs()->remove(path);
    auto [ok, h] = fs()->openFile(path, FileSystem::kWriteCreateNew);
    if (!ok)
        return false;
    int32_t len = (int32_t)strlen(text);
    bool good = fs()->writeFile(text, len, h) == len;
    fs()->closeFile(h);
    return good;
}

// kit.txt: one line per sound, "NN mode group cut color" (NN = the file's number,
// mode auto / oneshot / gate, choke group 0-31, cut on release 0 / 1, color 0-16);
// '#' lines and the "MPDJ kit" header are skipped
int applyPadSettings(const char* folderPath, int baseSlot, int count)
{
    static char text[4096];
    char path[256];
    snprintf(path, sizeof path, "%s/%s", folderPath, kInfoFile);
    if (!readText(path, text, sizeof text))
        return 0;
    int changed = 0;
    char* save = nullptr;
    for (char* line = strtok_r(text, "\r\n", &save); line; line = strtok_r(nullptr, "\r\n", &save))
    {
        while (*line == ' ' || *line == '\t')
            line++;
        if (!isdigit((unsigned char)*line))
            continue;
        int pos = 0, group = 0, cut = 0, color = 0;
        char word[16] = { 0 };
        if (sscanf(line, "%d %15s %d %d %d", &pos, word, &group, &cut, &color) < 2)
            continue;
        if (pos < 1 || pos > count)
            continue;
        int mode = kPadModeAuto;
        for (int m = 0; m < kPadModeCount; m++)
            if (strcmp(word, kModeWord[m]) == 0)
                mode = m;
        if (group < 0 || group > kPadGroups)
            group = 0;
        if (color < 0 || color > kPadColorChoices)
            color = 0;
        uint8_t slot = (uint8_t)(baseSlot + pos - 1);
        for (int p = 0; p < kPads; p++)
        {
            if (mtProject.pads.pad[p].program != slot)
                continue;
            mtProject.pads.pad[p].mode = padModeByte(PadMode(mode), (uint8_t)group, cut != 0);
            mtProject.pads.color[p] = (uint8_t)color;
            changed++;
        }
    }
    if (changed)
        newFileManager.setProjectStructChanged();
    printf("kits: %s -> slots %d-%d, pad settings on %d pads\n", folderPath, baseSlot + 1, baseSlot + count,
           changed);
    return changed;
}

// the program's name for a file: the sample's file name without its extension,
// without characters a file name can't hold
static void programFileName(int slot, char* out, int size)
{
    const char* src = mtProject.instrument[slot].sample.file_name;
    int n = 0;
    for (const char* s = src; *s && n < size - 1; s++)
    {
        char c = *s;
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|')
            c = '_';
        out[n++] = c;
    }
    out[n] = 0;
    char* dot = strrchr(out, '.');
    if (dot && dot != out)
        *dot = 0;
    // a leading "NN " (MPDJ Kit samples, earlier kits) gives way to the kit's own number
    if (isdigit((unsigned char)out[0]) && isdigit((unsigned char)out[1]) && out[2] == ' ' && out[3])
        memmove(out, out + 3, strlen(out + 3) + 1);
    if (!*out)
        snprintf(out, size, "Program %d", slot + 1);
}

int saveBank(int bank, const char* name, char* msg, int msgSize)
{
    char dir[256];
    snprintf(dir, sizeof dir, "%s/%s", kFolder, name);
    if (fs()->exists(dir))
        fs()->removeDirWithFiles(dir);
    if (!fs()->mkdir(0, dir, true))
    {
        snprintf(msg, msgSize, "Can't make %s", dir);
        return 0;
    }

    static char info[4096];
    int len = snprintf(info, sizeof info,
                       "MPDJ kit 1\n# NN mode group cut color (mode auto / oneshot / gate, "
                       "choke group 0-31, cut on release 0 / 1, color 0 = Auto, 1-16, 17 = Red)\n");
    int saved = 0;
    for (int i = 0; i < kPadsPerBank; i++)
    {
        int slot = bank * kPadsPerBank + i;
        if (!mtProject.instrument[slot].isActive || isMidiInstrument(slot))
            continue;
        newFileManager.saveInstrument((uint8_t)slot); // the workspace file = the program as it is now
        char src[128], prog[40], dst[320];
        snprintf(src, sizeof src, cWorkspaceInstrumentFileFormatNew, slot + 1);
        programFileName(slot, prog, sizeof prog);
        snprintf(dst, sizeof dst, "%s/%02d %s.pti", dir, saved + 1, prog);
        if (!cFileTransfer::copyFile(fs(), src, dst))
        {
            snprintf(msg, msgSize, "Couldn't copy program %d", slot + 1);
            return saved;
        }
        saved++;
        // pad settings: this bank's pad for the slot if it points there, else the
        // first pad in bank order that does
        int pad = -1;
        if (mtProject.pads.pad[slot].program == slot)
            pad = slot;
        for (int p = 0; pad < 0 && p < kPads; p++)
            if (mtProject.pads.pad[p].program == slot)
                pad = p;
        uint8_t mode = pad >= 0 ? mtProject.pads.pad[pad].mode : 0;
        uint8_t color = pad >= 0 ? mtProject.pads.color[pad] : 0;
        len += snprintf(info + len, sizeof info - len, "%02d %s %d %d %d\n", saved,
                        kModeWord[padModeOf(mode) < kPadModeCount ? padModeOf(mode) : 0],
                        padGroupOf(mode), (mode & kPadCutOnRelease) ? 1 : 0, color);
    }
    if (!saved)
    {
        fs()->removeDirWithFiles(dir);
        snprintf(msg, msgSize, "Bank %c has no samples", 'A' + bank);
        return 0;
    }
    char infoPath[300];
    snprintf(infoPath, sizeof infoPath, "%s/%s", dir, kInfoFile);
    writeText(infoPath, info);
    snprintf(msg, msgSize, "Kit %s saved: %d sounds", name, saved);
    return saved;
}
} // namespace mpdjKits
