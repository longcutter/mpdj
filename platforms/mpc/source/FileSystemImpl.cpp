#include "FileSystemImpl.h"
#include "util_string.h"

#include <algorithm>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace
{
struct PosixFile
{
    FILE* f = nullptr;
};

PosixFile* handle(FileSystem::FileHandle h)
{
    return static_cast<PosixFile*>(h);
}

bool isHiddenName(const char* name)
{
    return name[0] == '.';
}

bool isDir(const std::string& p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// Deletes everything inside `p`; also deletes `p` itself when removeSelf is set.
bool deleteTree(const std::string& p, bool removeSelf)
{
    bool ok = true;
    if (DIR* d = opendir(p.c_str()))
    {
        while (dirent* e = readdir(d))
        {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
                continue;
            std::string child = p + "/" + e->d_name;
            if (isDir(child))
                ok = deleteTree(child, true) && ok;
            else
                ok = (unlink(child.c_str()) == 0) && ok;
        }
        closedir(d);
    }
    else
        return false;
    if (removeSelf)
        ok = (rmdir(p.c_str()) == 0) && ok;
    return ok;
}

} // namespace

void FileSystemImpl::init()
{
    const char* r = getenv("MPDJ_ROOT");
    root = r && *r ? r : "/media/MPC SDC/MPDJ";
    while (root.size() > 1 && root.back() == '/')
        root.pop_back();
    if (!isDir(root))
    {
        // Create the folder if its parent (the SD card) is there.
        std::string parent = root.substr(0, root.find_last_of('/'));
        if (!parent.empty() && isDir(parent))
            ::mkdir(root.c_str(), 0755);
    }
    printf("files: %s%s\n", root.c_str(), isDir(root) ? "" : " (missing: no SD card)");
}

std::string FileSystemImpl::full(const char* path) const
{
    while (*path == '/')
        path++;
    std::string p = root;
    if (*path)
    {
        p += '/';
        p += path;
    }
    while (p.size() > 1 && p.back() == '/')
        p.pop_back();
    return p;
}

bool FileSystemImpl::isSdCardInserted()
{
    return isDir(root);
}

bool FileSystemImpl::exists(const char* path)
{
    struct stat st;
    return stat(full(path).c_str(), &st) == 0;
}

bool FileSystemImpl::rename(const char* oldPath, const char* newPath)
{
    return ::rename(full(oldPath).c_str(), full(newPath).c_str()) == 0;
}

bool FileSystemImpl::mkdir(uint8_t, const char* path, bool pFlag)
{
    std::string p = full(path);
    if (isDir(p))
        return true;
    if (pFlag)
    {
        for (size_t i = root.size() + 1; i < p.size(); i++)
            if (p[i] == '/')
            {
                std::string part = p.substr(0, i);
                if (!isDir(part) && ::mkdir(part.c_str(), 0755) != 0 && errno != EEXIST)
                    return false;
            }
    }
    return ::mkdir(p.c_str(), 0755) == 0 || errno == EEXIST;
}

bool FileSystemImpl::remove(const char* path)
{
    std::string p = full(path);
    return isDir(p) ? rmdir(p.c_str()) == 0 : unlink(p.c_str()) == 0;
}

bool FileSystemImpl::removeDirWithFiles(const char* path)
{
    return deleteTree(full(path), true);
}

bool FileSystemImpl::removeOnlyDirFiles(const char* path)
{
    return deleteTree(full(path), false);
}

size_t FileSystemImpl::getFileSize(const char* path)
{
    struct stat st;
    std::string p = full(path);
    if (stat(p.c_str(), &st) != 0 || S_ISDIR(st.st_mode))
        return 0;
    return (size_t)st.st_size;
}

size_t FileSystemImpl::getFileSize(FileHandle h)
{
    PosixFile* pf = handle(h);
    if (!pf || !pf->f)
        return 0;
    struct stat st;
    return fstat(fileno(pf->f), &st) == 0 ? (size_t)st.st_size : 0;
}

size_t FileSystemImpl::getFileAvailable(FileHandle h)
{
    PosixFile* pf = handle(h);
    if (!pf || !pf->f)
        return 0;
    long pos = ftell(pf->f);
    size_t size = getFileSize(h);
    return pos < 0 || (size_t)pos > size ? 0 : size - (size_t)pos;
}

std::tuple<bool, FileSystem::FileHandle> FileSystemImpl::openFile(const char* path,
                                                                  FileOpenMode mode,
                                                                  FileHandle* fileHandle)
{
    PosixFile* pf;
    if (!fileHandle)
        pf = new PosixFile;
    else if (!*fileHandle)
        *fileHandle = pf = new PosixFile;
    else
        pf = handle(*fileHandle);

    if (pf->f)
    {
        fclose(pf->f);
        pf->f = nullptr;
    }

    std::string p = full(path);
    switch (mode)
    {
        case kWriteOpen:
            pf->f = fopen(p.c_str(), "r+b");
            if (!pf->f)
                pf->f = fopen(p.c_str(), "w+b");
            break;
        case kWriteAppend:
            pf->f = fopen(p.c_str(), "r+b");
            if (!pf->f)
                pf->f = fopen(p.c_str(), "w+b");
            if (pf->f)
                fseek(pf->f, 0, SEEK_END);
            break;
        case kWriteCreateNew:
            pf->f = fopen(p.c_str(), "w+b");
            break;
        default:
            if (!isDir(p))
                pf->f = fopen(p.c_str(), "rb");
            break;
    }

    bool ok = pf->f != nullptr;
    if (!ok && !fileHandle)
    {
        delete pf;
        pf = nullptr;
    }
    return { ok, (FileHandle)pf };
}

int32_t FileSystemImpl::readFile(void* dest, size_t len, FileHandle h)
{
    PosixFile* pf = handle(h);
    if (!pf || !pf->f)
        return -1;
    size_t n = fread(dest, 1, len, pf->f);
    return ferror(pf->f) ? -1 : (int32_t)n;
}

int32_t FileSystemImpl::writeFile(const void* data, size_t len, FileHandle h)
{
    PosixFile* pf = handle(h);
    if (!pf || !pf->f)
        return -1;
    return (int32_t)fwrite(data, 1, len, pf->f);
}

bool FileSystemImpl::seekFileCur(int offset, FileHandle h)
{
    PosixFile* pf = handle(h);
    return pf && pf->f && fseek(pf->f, offset, SEEK_CUR) == 0;
}

bool FileSystemImpl::seekFile(uint32_t pos, FileHandle h)
{
    PosixFile* pf = handle(h);
    return pf && pf->f && fseek(pf->f, (long)pos, SEEK_SET) == 0;
}

void FileSystemImpl::closeFile(FileHandle h, bool deleteOnClose)
{
    PosixFile* pf = handle(h);
    if (!pf)
        return;
    if (pf->f)
    {
        fclose(pf->f);
        pf->f = nullptr;
    }
    if (deleteOnClose)
        delete pf;
}

void FileSystemImpl::endFile(FileHandle h)
{
    // Truncate at the current position, like SdFile::end().
    PosixFile* pf = handle(h);
    if (!pf || !pf->f)
        return;
    fflush(pf->f);
    long pos = ftell(pf->f);
    if (pos >= 0 && ftruncate(fileno(pf->f), pos) != 0)
        fprintf(stderr, "files: truncate failed\n");
}

bool FileSystemImpl::isFileOpen(FileHandle h)
{
    PosixFile* pf = handle(h);
    return pf && pf->f;
}

int FileSystemImpl::countContainedFiles(const char* path)
{
    // Folders plus .wav files, skipping hidden items (as SdDir::countContainedFiles).
    std::string p = full(path);
    DIR* d = opendir(p.c_str());
    if (!d)
        return 0;
    int n = 0;
    while (dirent* e = readdir(d))
    {
        if (isHiddenName(e->d_name))
            continue;
        if (!isDir(p + "/" + e->d_name))
        {
            size_t len = strlen(e->d_name);
            if (len < 5 || strcasecmp(e->d_name + len - 4, ".wav") != 0)
                continue;
            if (strlen(path) + len > 253)
                continue;
        }
        n++;
    }
    closedir(d);
    return n;
}

void FileSystemImpl::listDirContents(const char* dirpath,
                                     DirContentType type,
                                     std::function<bool(const char*)> nameFilter,
                                     char* namesBuffer,
                                     int namesBufferSize,
                                     int maxNameLen,
                                     int& count,
                                     char** fileList)
{
    std::string p = full(dirpath);
    DIR* d = opendir(p.c_str());
    if (!d)
    {
        count = 0;
        return;
    }

    int listPos = 0;
    while (listPos < count)
    {
        dirent* e = readdir(d);
        if (!e)
            break;
        const char* name = e->d_name;
        if (isHiddenName(name))
            continue; // also skips "." / ".." and macOS "._" files
        bool folder = isDir(p + "/" + name);
        if (type == kFolders && !folder)
            continue;
        if (type == kFiles && folder)
            continue;
        if (nameFilter != nullptr && !nameFilter(name))
            continue;

        int nameLen = (int)strlen(name) + (folder ? 2 : 1);
        if (nameLen > namesBufferSize)
            break;
        if (nameLen > maxNameLen)
            continue;

        fileList[listPos] = namesBuffer;
        char* out = namesBuffer;
        if (folder)
            *out++ = '/';
        strcpy(out, name);
        namesBuffer += nameLen;
        namesBufferSize -= nameLen;
        listPos++;
    }
    count = listPos;
    closedir(d);
    std::sort(fileList,
              fileList + count,
              [](const char* a, const char* b) { return compareStrings(a, b); });
}

int FileSystemImpl::readNbFiles(const char* path)
{
    std::string p = full(path);
    DIR* d = opendir(p.c_str());
    if (!d)
        return 0;
    int n = 0;
    while (dirent* e = readdir(d))
        if (!isHiddenName(e->d_name) && !isDir(p + "/" + e->d_name))
            n++;
    closedir(d);
    return n;
}

bool FileSystemImpl::nesDir_open(const char* path)
{
    nesDir_close();
    nesDirPath = full(path);
    nesDir = opendir(nesDirPath.c_str());
    return nesDir != nullptr;
}

bool FileSystemImpl::nesDir_readItem(char* path, uint8_t* isDirOut, uint8_t* isHid, uint32_t* size)
{
    if (!nesDir)
        return false;
    dirent* e;
    do
    {
        e = readdir((DIR*)nesDir);
        if (!e)
            return false;
    } while (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."));
    std::string child = nesDirPath + "/" + e->d_name;
    struct stat st;
    bool ok = stat(child.c_str(), &st) == 0;
    strcpy(path, e->d_name);
    if (isDirOut)
        *isDirOut = ok && S_ISDIR(st.st_mode);
    if (isHid)
        *isHid = isHiddenName(e->d_name);
    if (size)
        *size = ok ? (uint32_t)st.st_size : 0;
    return true;
}

void FileSystemImpl::nesDir_close()
{
    if (nesDir)
        closedir((DIR*)nesDir);
    nesDir = nullptr;
}
