#ifndef FILEMANAGER_RECORDINGIMPORT_H_
#define FILEMANAGER_RECORDINGIMPORT_H_

#include "FileManagerOperation.h"

#include "Platform.h"

#include <optional>
#include <stdint.h>

class RecordingSave : public FileManagerOperation
{
  public:
    virtual void update() override;

    bool saveRecordedSound(char* filePath,
                           char* fileName,
                           std::optional<int8_t> instrIdx,
                           const int16_t* leftChannelAddress,
                           const int16_t* rightChannelAddress,
                           uint32_t length);
    virtual float getProgress() override;

  private:
    enum RecordingImportStep
    {
        saveRecordedFile,
        saveInstrumentStruct,
        copySampleData,
        finishStep,
    };

    struct SdCardFileSaveContext
    {
        FileSystem::FileHandle file = nullptr;
        std::vector<const int16_t*> soundAddress;
        uint32_t soundLength;
        bool isActive;
        uint32_t savedSoundData;
    };

    struct InstrumentSaveContext
    {
        int importInstrumentIdx;
        uint8_t* memoryAddressR;
        uint8_t* memoryAddressL;
        uint32_t channelDataSizeBytes;
        uint32_t length; // MPDJ: frames of the sound being saved (was the recorder's own)
    };

    bool saveRecording();
    bool updateSave();
    void finishSave();
    void prepareInstrumentStruct(int idx, char* name);
    bool saveStereoDataToWorkspace(int idx,
                                   uint8_t* dataSourceAddressL,
                                   uint8_t* dataSourceAddressR,
                                   uint32_t dataSizeBytes);
    void saveRecordingFinish();
    SdCardFileSaveContext sdCardFileSaveContext;
    InstrumentSaveContext instrumentSaveContext;

    RecordingImportStep operationStep;
};

#endif /* FILEMANAGER_RECORDINGIMPORT_H_ */
