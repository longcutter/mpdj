// POSIX file system for the MPC One+. The Tracker's "SD card" is a folder:
// MPDJ_ROOT, default "/media/MPC SDC/MPDJ" (the MPC's SD card).
//
// Milestone 2 scope: files, folders and listDirContents work. Still to come with
// milestone 5: createFilesList / createInstrumentsList (file browser lists) and
// the raw-sector async reads used for sample streaming.
#pragma once

#include "Platform.h"

#include <string>

class FileSystemImpl : public FileSystem
{
  public:
    void init();
    const char* rootPath() const { return root.c_str(); }

    bool isSdCardInserted() override;
    void setMountEventCallback(CardStateChangeCallback cb) override { callback = cb; }
    bool remount() override { return isSdCardInserted(); }

    bool exists(const char* path) override;
    bool rename(const char* oldPath, const char* newPath) override;
    bool mkdir(uint8_t hidden, const char* path, bool pFlag) override;
    bool remove(const char* path) override;
    bool removeDirWithFiles(const char* path) override;
    bool removeOnlyDirFiles(const char* path) override;

    size_t getFileSize(const char* path) override;
    size_t getFileSize(FileHandle file) override;
    size_t getFileAvailable(FileHandle file) override;

    std::tuple<bool, FileHandle> openFile(const char* path,
                                          FileOpenMode mode,
                                          FileHandle* fileHandle) override;
    int32_t readFile(void* dest, size_t len, FileHandle file) override;
    int32_t writeFile(const void* data, size_t len, FileHandle file) override;
    bool seekFileCur(int offset, FileHandle file) override;
    bool seekFile(uint32_t pos, FileHandle file) override;
    void closeFile(FileHandle file, bool deleteOnClose) override;
    void endFile(FileHandle file) override;
    bool isFileOpen(FileHandle file) override;

    // Raw SD sector access has no equivalent here yet (sample streaming, milestone 4/5).
    bool enableFastSeek(FileHandle, uint32_t*, uint32_t, uint32_t*, uint32_t*) override
    {
        return false;
    }
    bool readSectorsAsync(uint8_t*, uint32_t, uint32_t) override { return false; }
    bool pollAsyncRead(bool* ok) override
    {
        if (ok)
            *ok = false;
        return true;
    }
    bool waitAsyncRead() override { return false; }
    bool isAsyncReadBusy() override { return false; }

    int createInstrumentsList(const char*, char**, uint8_t, uint32_t*) override { return 0; }
    int countContainedFiles(const char* path) override;
    int createFilesList(const char*,
                        uint8_t,
                        char**,
                        uint8_t,
                        uint16_t,
                        enListFileFilter,
                        uint16_t,
                        uint16_t,
                        enListFileFilter) override
    {
        return 0;
    }
    void listDirContents(const char* dirpath,
                         DirContentType type,
                         std::function<bool(const char*)> nameFilter,
                         char* namesBuffer,
                         int namesBufferSize,
                         int maxNameLen,
                         int& outNamesCount,
                         char** fileList) override;

    int readNbFiles(const char* path) override;
    bool nesDir_open(const char* path) override;
    bool nesDir_readItem(char* path, uint8_t* isDir, uint8_t* isHid, uint32_t* size) override;
    void nesDir_close() override;

  private:
    std::string full(const char* path) const;

    std::string root;
    CardStateChangeCallback callback = nullptr;
    void* nesDir = nullptr;
    std::string nesDirPath;
};
