#ifndef EXPORTER_MTAUDIOEXPORT_H_
#define EXPORTER_MTAUDIOEXPORT_H_

#include "AudioStream.h"
#include "Platform.h"
#include "elapsedMillis.h"
#include "wavHeaderReader.h"

enum enExportType
{
    exportSong = 0,
    exportSongStems,
    exportPattern,
    exportPatternStems,

    exportPatternSelection,

    exportTypesCount
};

const uint8_t numOfAudioBlocksInPacket = 70; // ile blokow w buforze na 1 stem

class mtAudioExport
{

  public:
    enum exportStatus
    {
        statusIdle,
        statusRender,

        statusRawToWavs_init,
        statusRawToWavs_startWav,
        statusRawToWavs_update,
        statusRawToWavs_finish,
    };

    void init(FileSystem* fileSystem);
    void update();

    void start(const char* path, uint8_t mode);
    void cancel();

    uint8_t getStatus();
    uint8_t getProgress();
    void setOnLastStep();
    uint32_t getRenderLength();

    enum enStem
    {
        stem_tracks_start,
        stem_track_1 = stem_tracks_start,
        stem_track_2,
        stem_track_3,
        stem_track_4,
        stem_track_5,
        stem_track_6,
        stem_track_7,
        stem_track_8,
        // MPDJ (step 5a-4): tracks 9-16 have voices too
        stem_track_9,
        stem_track_10,
        stem_track_11,
        stem_track_12,
        stem_track_13,
        stem_track_14,
        stem_track_15,
        stem_track_16,

#ifdef FEATURE_SYNTH
        stem_synth_1,
        stem_synth_2,
        stem_synth_3,
#endif

        stem_reverb,
        stem_delay,

        stem_master,

        stem_totalCount,
    };

  private:
    FileSystem* fileSystem = nullptr;

    elapsedMillis totalExportTimer = 0;

    uint16_t stemSelectionMask = 0b0000011111111111;

    uint32_t bytesWrittenInStem[stem_totalCount];
    uint16_t bufferedBlocks[stem_totalCount] = { 0 };

    uint8_t maxStemInCurrentExport = 0;

    uint32_t seqTimerPeriod = 0;
    uint32_t seqTimerLastExecutionTime = 0;
    float seqTimerRemainder = 0.0;

    bool lastSequenceStep = false;
    enExportType selectedMode = exportPattern;

    uint16_t songLength = 1;

    uint16_t maxBlocksForStem = numOfAudioBlocksInPacket;

    uint32_t multipacketsInRawTotal = 0; // ile multipaczek zrzucone do SD
    uint32_t lastMultipacketBlocksCount = 0;

    int consecutiveEmptyBuffers = 0;
    int fxTimeBuffers;
    int buffersAfterStop;

    bool renderFinished = false;

    static constexpr float kAudioSampleRate = 44100.0f;

    static constexpr int kMaxExportTimeAfterStopSequenceSec = 60;
    static constexpr int kMaxBuffersAfterStop =
      (kMaxExportTimeAfterStopSequenceSec * kAudioSampleRate) / AUDIO_BLOCK_SAMPLES;

    void updateRender();
    void rawToWavs_init();
    void rawToWavs_startWav();
    void rawToWavs_update();
    void rawToWavs_finish();
    void handleStem(uint8_t stemIdx);
    void handleError(uint16_t);

    bool isStemActive(int stemIdx);
    bool exportOnlyMaster();

    void saveMasterBufferToSD(uint8_t);
    void saveRawStemsBufferToSD(uint8_t closeFile);

    bool isBufferSilent(const int16_t* buffer);

    void setExternalSettings();
    void callBackExternalSettings();

    void finishRender();
    void runSequence();

    float getRawToWavProgress();

    struct strRawToWav
    {
        uint8_t stemIndex = 0;

        uint32_t currentMultipacket = 0;
        uint32_t totalPacketProcessed = 0;

    } raw;

    exportStatus status;
};

extern mtAudioExport exporter;

#endif /* EXPORTER_MTAUDIOEXPORT_H_ */
