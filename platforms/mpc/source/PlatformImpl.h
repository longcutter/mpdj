// MPC One+ platform layer for Polyend Tracker CE.
// Milestone 1: every class exists so the tracker links and starts.
// Pieces marked STUB get real implementations in later milestones.
#pragma once

#include "ConfigManager.h"
#include "DisplayDriverImpl.h"
#include "FileSystemImpl.h"
#include "audio/MpcAudio.h"
#include "input/MpcInput.h"
#include "Platform.h"

#include <stdint.h>

// ---------------------------------------------------------------- timer
class TimerImpl : public Timer
{
  public:
    uint32_t millis() override;
    uint32_t micros() override;
    void delay(int ms) override;
    uint32_t clockCycles() override;
    uint32_t clockCyclesPerUs() override { return 1; }
};

// ---------------------------------------------------------------- buttons / pads
class SwitchGroupImpl : public SwitchGroup
{
  public:
    explicit SwitchGroupImpl(int count) : count(count) {}
    void setHoldTime(int timeMs) override { holdTimeMs = timeMs; }
    bool isButtonPressed(int idx) override;
    void setActionCallback(ActionType action, void (*callback)(uint8_t)) override;

    // called by the MPC input layer
    void press(int idx);
    void release(int idx);
    void update(); // fires hold events

  private:
    static constexpr int kMax = 64;
    int count;
    int holdTimeMs = 500;
    bool pressed[kMax] = {};
    bool holdSent[kMax] = {};
    uint32_t pressTime[kMax] = {};
    void (*onPress)(uint8_t) = nullptr;
    void (*onRelease)(uint8_t) = nullptr;
    void (*onHold)(uint8_t) = nullptr;
};

class EncoderImpl : public Encoder
{
  public:
    void setAcceleration(int a) override { acceleration = a; }
    void setResolution(int r) override { resolution = r; }
    void turn(int steps); // called by the MPC input layer
  private:
    int acceleration = 0;
    int resolution = 1;
};

// ---------------------------------------------------------------- config (file backed)
class ConfigManagerImpl : public ConfigManager
{
  public:
    static constexpr int kSize = 64 * 1024;
    void init(const char* path);
    bool readData(int address, void* data, int size) override;
    void writeData(int address, void* data, int size) override;

  private:
    const char* path = nullptr;
    uint8_t mem[kSize];
};

// ---------------------------------------------------------------- simple stubs
class CrcCalculatorImpl : public CrcCalculator
{
  public:
    uint32_t calculate32bitCrc(const uint8_t* data, const uint16_t len) override;
};

class LedsImpl : public Leds
{
  public:
    void setLed(int, bool, int) override {} // STUB: pad LEDs via MIDI later
};

class BatteryImpl : public Battery
{
  public:
    void enableMonitoring() override {}
    int getLevel() override { return 100; }
    float getVoltage() override { return 4.2f; }
    bool getCharging() override { return false; }
    int getTimeTo() override { return 0; }
    bool getState() override { return true; }
    bool isEmpty() override { return false; }
};

class RadioImpl : public Radio
{
  public:
    void enableLineInput(bool) override {}
    void enable(bool) override {}
    bool setFrequency(float f) override
    {
        freq = f;
        return false;
    }
    void setSeekCallback(void (*)(void)) override {}
    void seek(SeekDirection) override {}
    void updateSeek() override {}
    bool updateRDS() override { return false; }
    float getFrequency() override { return freq; }
    float getBaseFrequency() override { return 87.5f; }
    float getSpacing() override { return 0.1f; }
    bool isinitialized() override { return false; }
    void clearRDS() override {}
    void setRegion(Region) override {}
    bool isRegionChanged() override { return false; }
    char* getStationName() override { return name; }

  private:
    float freq = 87.5f;
    char name[2] = "";
};

class CodecImpl : public Sgtl5000
{
  public:
    void init() override {}
    void setInputType(InputType) override {}
    void setHeadphoneSource(HeadphoneSource) override {}
    void setLineInLevel(int) override {}
    void setMicGain(int) override {}
    void setHeadphoneMuted(bool m) override { MpcAudio::setHeadphoneMuted(m); }
    void setMasterVolume(float v) override
    {
        masterVolume = v;
        MpcAudio::setVolume(v);
    }
    void setAutoVolumeEnabled(bool) override {}
    void configureAutoVolume(AutoVolumeParams*) override {}
    void setEnhanceBassEnabled(bool) override {}
    void configureEnhanceBass(EnhanceBassParams*) override {}
    void setEqualizerType(EqualizerType) override {}
    void configureFiveBandEqualizer(float*) override {}
    void enableSurroundSound(bool) override {}
    void configureSurroundSound(SurroundSoundParams*) override {}
    void enableHardwareProcessor(bool) override {}
    float masterVolume = 1.0f;
};

class MidiStub
{
  public:
    bool isReadAvailable() { return false; }
};

class UsbMidiImpl : public UsbMidi
{
  public:
    bool isReadAvailable() override { return false; }
    void read() override {}
    void sendRealTime(MessageType) override {}
    void sendControlChange(uint8_t, uint8_t, uint8_t) override {}
    void sendProgramChange(uint8_t, uint8_t) override {}
    void sendAfterTouch(uint8_t, uint8_t) override {}
    void sendPolyPressure(uint8_t, uint8_t, uint8_t) override {}
    void sendPitchBend(int, uint8_t) override {}
    void sendNoteOn(uint8_t, uint8_t, uint8_t) override {}
    void sendNoteOff(uint8_t, uint8_t, uint8_t) override {}
    void setReceiveActions(MidiReceiveActions*) override {}
};

class UartMidiImpl : public UartMidi // STUB: DIN MIDI on hw:0,0,2 later
{
  public:
    bool isReadAvailable() override { return false; }
    void read() override {}
    void sendRealTime(MessageType) override {}
    void sendControlChange(uint8_t, uint8_t, uint8_t) override {}
    void sendProgramChange(uint8_t, uint8_t) override {}
    void sendAfterTouch(uint8_t, uint8_t) override {}
    void sendPolyPressure(uint8_t, uint8_t, uint8_t) override {}
    void sendPitchBend(int, uint8_t) override {}
    void sendNoteOn(uint8_t, uint8_t, uint8_t) override {}
    void sendNoteOff(uint8_t, uint8_t, uint8_t) override {}
    void init() override {}
    void setReceiveActions(MidiReceiveActions*) override {}
    void setReceiveNoteOnAction(void (*)(uint8_t, uint8_t, uint8_t)) override {}
};

class UsbAudioImpl : public UsbAudio
{
  public:
    void beginFillData() override {}
    void fillChannelData(void*, int, int) override {}
    void finishFillData() override {}
};

class UsbImpl : public Usb
{
  public:
    void init() override {}
    void reset() override {}
    void update() override {}
    void setClassState(ClassType, bool) override {}
    bool getClassState(ClassType) override { return false; }
    bool isAttached() override { return false; }
};

// ---------------------------------------------------------------- platform
class PlatformImpl : public Platform
{
  public:
    PlatformImpl();
    void init();
    void update();

    void setPowerMode(bool mode) override;
    void resetMCU() override;
    void runBootloader(char*) override {}
    void setPowerButtonCallback(PowerButtonCallback cb) override { powerCallback = cb; }
    bool getPowerButtonState() override { return false; }
    bool readForceStartUp() override { return false; }

    Sgtl5000* getCodecAudio() override { return &codec; }
    Radio* getRadio() override { return &radio; }
    SwitchGroup* getButtons() override { return &buttons; }
    SwitchGroup* getPads() override { return &pads; }
    Leds* getLeds() override { return &leds; }
    Battery* getBattery() override { return &battery; }
    Timer* getTimer() override { return &timer; }
    Encoder* getEncoder() override { return &encoder; }
    DisplayDriver* getDisplayDriver() override { return &display; }
    UsbAudio* getUsbAudio() override { return &usbAudio; }
    UartMidi* getUartMidi() override { return &uartMidi; }
    UsbMidi* getUsbMidi() override { return &usbMidi; }
    ConfigManager* getConfigManager() override { return &config; }
    FileSystem* getFileSystem() override { return &fileSystem; }
    CrcCalculator* getCrcCalculator() override { return &crc; }
    SystemInfo* getSystemInfo() override { return &systemInfo; }
    Usb* getUsb() override { return &usb; }

    SwitchGroupImpl buttons;
    SwitchGroupImpl pads;
    EncoderImpl encoder;
    DisplayDriverImpl display;
    MpcInput input;
    FileSystemImpl fileSystem;

  private:
    CodecImpl codec;
    RadioImpl radio;
    LedsImpl leds;
    BatteryImpl battery;
    TimerImpl timer;
    UsbAudioImpl usbAudio;
    UartMidiImpl uartMidi;
    UsbMidiImpl usbMidi;
    ConfigManagerImpl config;
    CrcCalculatorImpl crc;
    SystemInfo systemInfo;
    UsbImpl usb;
    PowerButtonCallback powerCallback = nullptr;
};

extern PlatformImpl platform;
extern volatile bool g_running;
