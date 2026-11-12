#ifndef LIBS_NEWDISPLAY_DISPLAY_H_
#define LIBS_NEWDISPLAY_DISPLAY_H_

#include <stdint.h>
#include <typeinfo>

#include "CompressionRLE.h"
#include "displayControls.h"

#include "barControl.h"
#include "batteryControl.h"
#include "boxMenuControl.h"
#include "commonControls.h"
#include "gridEditorControl.h"
#include "horizontalBarControl.h"
#include "horizotnalTrackerControl.h"
#include "imgControl.h"
#include "imgGridControl.h"
#include "keyboardControl.h"
#include "lineIndicatorControl.h"
#include "list.h"
#include "multiLabelControl.h"
#include "newListControl.h"
#include "notePopoutControl.h"
#include "notesKeyboard.h"
#include "paramValueList.h"
#include "pointsControl.h"
#include "progressCursorControl.h"
#include "simpleBox.h"
#include "simpleTextControl.h"
#include "sliceControl.h"
#include "songPlayerControl.h"
#include "spectrumControl.h"
#include "startScreenControl.h"
#include "textBoxControl.h"
#include "textPopupControl.h"
#include "trackerControl.h"
#include "wtProgress.h"

#include "Themes.h"
#include "displayStructs.h"

#include "DisplayDriver.h"

// #########################################################################
//							KLASY
// #########################################################################

struct CompressedFontData
{
    const CompressedData* data;
    int count;
};

class FileSystem;

class cDisplay
{
    friend cDebugLog;

  public:
    cDisplay();
    void begin(DisplayDriver* displayDriver, Timer* timer);
    void update();

    template<typename controlClass>
    [[gnu::noinline]] hControl createControl(strControlProperties* properties)
    {
        cDisplayControl** newControlSlot = &controlsTable[0];

        uint8_t limit = 0;

        while (nullptr != *newControlSlot)
        {
            newControlSlot++;
            if (++limit >= controlsCount)
                return -1;
        }

        if (((properties->style >> 8) & 15) > displayFontCount)
        {
            properties->style &= ~(15 << 8);
        }

        *newControlSlot = new controlClass(properties);

        return limit;
    }

    void destroyControl(hControl handle);
    // MPDJ: read-only access to a control (touch hit-testing on the MPC)
    cDisplayControl* mpdjControl(hControl handle) const
    {
        return (handle >= 0 && handle < (hControl)controlsCount) ? controlsTable[handle] : nullptr;
    }
    void refreshControl(hControl handle);
    void synchronizeRefresh();
    void resetControlQueue();
    uint8_t isIdle();
    void forceAppedStage();
    void processDisplayUpdateImmediate();
    bool isControlsRefreshProcessIdle();

    void setControlPosition(hControl handle, int16_t x, int16_t y);
    void setControlSize(hControl handle, int16_t w, int16_t h);
    void setControlStyle(hControl handle, uint32_t style);
    void setControlStyle2(hControl handle, uint32_t style);
    void setControlShow(hControl handle);
    void setControlHide(hControl handle);
    void setAddControlStyle(hControl handle, uint32_t style);
    void setRemoveControlStyle(hControl handle, uint32_t style);
    void setControlText(hControl handle, const char* text);
    void setControlText2(hControl handle, const char* text);

    void setControlValue(hControl handle, int value);
    void setControlColors(hControl handle, const uint32_t colorsTable[]);
    void setControlDefaultColors(hControl handle, uint32_t colorsTable[]);
    void setControlData(hControl handle, void* data);

    void setClearRefreshFlag(hControl handle);

    void setModulesControlDestroyPoint();
    void destroyModulesControls();
    void freeControlResources(int8_t controlsTableIndex);

    // jpg/png
    uint8_t readImgFromSd(char* path, uint32_t address, int16_t max_w, int16_t max_h);
    void readImgFromMemory(uint8_t* data, uint32_t size);
    uint8_t isImgLoaded() { return img.status == 3; }
    uint8_t getImgLoadState() { return img.status; }

    void scheduleScreenShot(int timeSec);
    void doScreenShot();

    void hideAllControls();

    // debug
    void printActualControlTableContent();
    void controlIdToString(uint8_t id, char* string);

    // hardware
    void setBacklightBrightness(uint8_t value);
    void setRotate(uint8_t value);

    void disable() { state = 0; }
    void enable() { state = 1; }
    void clear();

    void turnOn();
    void turnOff();

    //
    void startVideoStream();
    void updateVideoStream(bool lastFrame);
    void stopVideoStream();

    bool showVideoStreamFrame();
    uint16_t* getLineBuffer(int j);
    void writeScreen(int width, int height, int stride, uint8_t* buf, uint16_t* palette16);
    void writeLine(int width,
                   int height,
                   int stride,
                   uint8_t* buf,
                   uint8_t* palette8,
                   uint16_t* palette16);
    static uint16_t* getFrameBuffer(void);

    void setTheme(uint8_t index);
    void setCustomTheme(const Theme& customTheme);
    void setFont(uint8_t index);
    bool loadCustomFontFromSD(FileSystem* fs, const char* fontFilePath);

    void showBatteryScreen(bool empty, bool charging, int8_t level, float voltage);

    DisplayDriver* getDisplayDriver() { return displayDriver; }

    Theme& getTheme() { return *((Theme*)theme); }

  private:
    void step1();
    void step2();
    void step3();

    bool validateCurrentUpdateData();

    void loadFont(const CompressedFontData* fontsData, uint8_t fontCount);
    uint8_t* decompressFont(const CompressedFontData* compressedFontData);

    DisplayDriver* displayDriver;
    Timer* timer;
    const Theme* theme;

    uint8_t state = 1;

    cDisplayControl* controlsTable[controlsCount] = { nullptr };
    cDisplayControl* actualUpdating;
    int8_t actualUpdatingIndex;
    uint8_t actualUpdatingStage;
    uint8_t updateStep;
    bool freshControl;

    bool stopAppend;
    uint8_t synchQueuePosition;
    hControl refreshQueue[controlsRefreshQueueSize]; // FIFO
    uint8_t refreshQueueTop;
    uint8_t refreshQueueBott;

    int8_t destroyPoint = 0;

    uint32_t memoryUsed = 0;
    uint32_t lastMemoryUsed = 0;

    uint8_t fontsCount = displayFontCount;
    int8_t controls_count = 0;

    uint8_t updateDisplaySettings = 0;

    uint8_t backlightBrightness = 255;
    uint8_t lastBacklightBrightness = 255;

    uint8_t rotateValue = 1;
    uint8_t lastRotateValue = 1;

    // png/jpg
    uint8_t loadImage = 0;

    struct imgLoader
    {
        uint8_t status = 0; // 0-1-2-3
        uint8_t loadProgress = 0;
        uint32_t progressMax = 0;
        uint8_t type;
        uint8_t* data;
        uint32_t size;

        uint32_t address;

    } img;

    bool screenShotscheduled = false;
    uint32_t screenShotTimer = false;
};

extern uint32_t ImageViewerJpegAddress;
extern cDisplay display;

#endif
