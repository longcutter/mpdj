#ifndef AUDIOENGINE_VOICESTREAMMANAGER_H_
#define AUDIOENGINE_VOICESTREAMMANAGER_H_

#include "Platform.h"
#include "config.h"
#include "mtStructs.h"
#include <stdint.h>

class VoiceStreamManager
{
  public:
    static constexpr int kVoiceCount = 16; // MPDJ (step 5a-4): a voice per track
    static constexpr int kMaxInstruments = kSampleInstrumentsCount; // MPDJ: 64 slots
#ifdef PLATFORM_IMX
    static constexpr int kMaxChannels = 2;
#else
    static constexpr int kMaxChannels = 1;
#endif
    static constexpr uint32_t kRingFrames = 16384; // 32 KB per channel (~371 ms ring buffer)
    static constexpr uint32_t kChunkFrames = 4096; // 8 KB (16 sectors) per async ADMA2 read
    static constexpr uint32_t kRingMask = kRingFrames - 1;
    static constexpr uint32_t kLoopWrapFrames = 2048; // 4 KB per channel loop wrap pre-cache
    static constexpr uint32_t kLookbehindFrames = 512;

    void init(FileSystem* fileSystem = nullptr);

    // Non-blocking background scheduler (call from main loop / updateSdStream)
    void update();

    // Synchronous pump for offline rendering (call from mtAudioExport::updateRender)
    void updateSynchronous();

    // For while something else, like a USB host, owns the card: update() reads
    // nothing from it. Voices keep playing what they have in RAM, then silence.
    void setSuspended(bool suspended);

    // Voice lifecycle notifications (safe to call from Audio ISR)
    void notifyNoteOn(int voiceIdx,
                      int instrIdx,
                      uint32_t startFrame,
                      uint32_t loopStartFrame,
                      uint32_t loopEndFrame,
                      uint32_t endFrame,
                      uint8_t playMode,
                      bool isReverse,
                      float pitchSpeed,
                      uint32_t sliceCachedFrames = 0);
    void notifyNoteOff(int voiceIdx);
    void invalidateInstrumentVoices(int instrIdx);

    AUDIO_CORE_RAM bool getSlicePreCache(int instrIdx,
                                         uint8_t sliceIdx,
                                         int channel,
                                         uint32_t actualStartFrame,
                                         const int16_t*& outAddr,
                                         uint32_t& outFrames) const;

    AUDIO_CORE_RAM void updateVoicePlayhead(int voiceIdx,
                                            uint32_t absFrame,
                                            int8_t direction,
                                            float pitchSpeed);

    AUDIO_CORE_RAM int16_t getSample(int voiceIdx,
                                     int channel,
                                     uint32_t absFrame,
                                     const int16_t* preCacheBase,
                                     uint32_t preCacheFrames);

    // Per-instrument EXTERNAL_RAM metadata accessors
    char* getInstrumentStreamPath(int instrIdx);
    const char* getInstrumentStreamPath(int instrIdx) const;
    uint32_t* getInstrumentClmt(int instrIdx);
    const uint32_t* getInstrumentClmt(int instrIdx) const;
    WaveformPeakPoint* getInstrumentWaveformPeaks(int instrIdx, int channel);
    const WaveformPeakPoint* getInstrumentWaveformPeaks(int instrIdx, int channel) const;

    void clearInstrumentMetadata(int instrIdx);
    void copyInstrumentMetadata(int srcInstrIdx, int dstInstrIdx);
    void buildWaveformPeaksFromRam(int instrIdx,
                                   int channel,
                                   const int16_t* samples,
                                   uint32_t totalFrames);
    bool setupInstrumentStream(int instrIdx,
                               const char* filePath,
                               uint32_t dataOffset,
                               uint32_t totalFrames,
                               bool isStereo);
    bool rebindInstrumentStreamFile(int instrIdx,
                                    const char* filePath,
                                    uint32_t dataOffset);
    // For after the card was remounted: the files may have moved on it.
    void rebindAllInstrumentStreamFiles();
    void refreshInstrumentSliceCache(int instrIdx);
    void refreshInstrumentSingleSliceCache(int instrIdx, uint8_t sliceIdx);

    // Blocking read of arbitrary frame range from a streamed instrument's backing file
    // (used for zoomed waveform view, Sample Editor staging, ProjectSave, and Sample Export)
    bool readInstrumentFramesBlocking(int instrIdx,
                                      int channel,
                                      uint32_t startFrame,
                                      uint32_t frameCount,
                                      int16_t* dest);

    // Translate file byte offset to physical SD card LBA sector using instrument's CLMT
    bool translateOffsetToLba(int instrIdx,
                              uint32_t fileByteOffset,
                              uint32_t& outLbaSector,
                              uint32_t& outMaxContigSectors) const;

  private:
    struct VoiceStreamState
    {
        volatile bool active = false;
        volatile uint8_t instrIdx = 0;
        volatile uint8_t playMode = 0;
        volatile int8_t direction = 1;
        volatile bool isStereo = false;
        volatile uint32_t startFrame = 0;
        volatile uint32_t endFrame = 0;
        volatile uint32_t loopStartFrame = 0;
        volatile uint32_t loopEndFrame = 0;
        volatile uint32_t preCacheFrames = 0;
        volatile uint32_t totalFrames = 0;
        volatile uint32_t currentFrame = 0;
        volatile float pitchSpeed = 1.0f;

        volatile uint32_t validStartFrame = 0;
        volatile uint32_t validEndFrame = 0;
        volatile uint32_t ringSampleHash = 0;

        volatile bool loopWrapReady = false;
        volatile uint32_t loopWrapStartFrame = 0;
        volatile uint32_t loopWrapEndFrame = 0;

        volatile uint32_t generation = 0;
        volatile uint16_t fadeInCounter = 32;
    };

    enum class AsyncTargetType : uint8_t
    {
        kNone = 0,
        kRingForward,
        kRingBackward,
        kLoopWrap
    };

    struct AsyncJob
    {
        bool inProgress = false;
        bool transferLaunched = false;
        int voiceIdx = -1;
        uint8_t instrIdx = 0;
        uint8_t channel = 0;
        uint8_t totalChannels = 1;
        AsyncTargetType targetType = AsyncTargetType::kNone;
        uint32_t fetchStartFrame = 0;
        uint32_t fetchFrameCount = 0;
        uint32_t intraSectorByteOffset = 0;
        uint32_t expectedGeneration = 0;
    };

    FileSystem* fs = nullptr;
    VoiceStreamState voices[kVoiceCount];
    AsyncJob currentJob;
    bool isSuspended = false;

    bool rebuildInstrumentClmt(int instrIdx);
    bool startAsyncSectorReadForJob(uint8_t channel);
    void completeChannelTransfer();
    bool scheduleNextVoiceJob(bool synchronous);
    void fillJobSynchronously(const AsyncJob& job);
};

extern VoiceStreamManager voiceStreamManager;

#endif /* AUDIOENGINE_VOICESTREAMMANAGER_H_ */
