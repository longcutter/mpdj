#include "titleBar.h"
#include "core/interfaceDefs.h"
#include "display.h"

#if GUI_HANDLE_SYNTH
#include "Profiler.h"
#include "Synth.h"
#endif

TitleBar titleBar;

static elapsedMillis refreshTimer;

#ifdef TRACKER_MINI
#define BATTERY_ENABLED 1
#endif

void TitleBar::init(Synth* synth, [[maybe_unused]] Battery* battery)
{
    this->synth = synth;
#ifdef BATTERY_ENABLED
    batteryDriver = battery;
    lastBatteryLevel = batteryDriver->getState() ? batteryDriver->getLevel() : kBatteryNotPresent;
#else
    batteryDriver = nullptr;
#endif
}

void TitleBar::initControls()
{
    strControlProperties prop;
    prop.x = 9;
    prop.y = 13;
    prop.w = 270;
    prop.colors = display.getTheme().topBarLabelColors;
    prop.style = (controlStyleCenterY | controlStyleFont4 | controlStyleLimitTextWidth);
    titleLabel = display.createControl<cLabel>(&prop);

    prop.y = 13;
#ifdef BATTERY_ENABLED
    prop.x = 790 - 28;
#else
    prop.x = 790;
#endif
    prop.colors = display.getTheme().topBarLabelColors;
    prop.style = (controlStyleRightX | controlStyleCenterY | controlStyleFont4);
    rightLabel = display.createControl<cLabel>(&prop);

#ifdef BATTERY_ENABLED
    prop.style = controlStyleShowBitmap | controlStyleCenterY;
    prop.colors = display.getTheme().batteryIconColors;
    prop.x = 795 - 28;
    prop.y = 13;
    prop.w = 28;
    prop.h = 13;
    prop.value = lastBatteryLevel;
    battery = display.createControl<cBattery>(&prop);
#endif

    prop.style = 0;
    prop.colors = &display.getTheme().red;
    prop.x = 297;
    prop.y = 0;
    prop.w = 170;
    prop.h = 1;
    prop.value = 0;
    performBox = display.createControl<SimpleBox>(&prop);

    barLabelData.line.color = display.getTheme().topBarLabelColors[2];
    barLabelData.line.x1 = 0;
    barLabelData.line.x2 = 800;
    barLabelData.line.y1 = 28;
    barLabelData.line.y2 = 28;

    prop.style = (controlStyleBackground | controlStyleSpecialFx);
    prop.colors = display.getTheme().topBarLabelColors;
    prop.x = 0;
    prop.y = 0;
    prop.w = 800;
    prop.h = 28;
    prop.value = 0;
    prop.data = &barLabelData;
    bar = display.createControl<cLabel>(&prop);
}

void TitleBar::refreshColors()
{
    display.setControlColors(titleLabel, display.getTheme().topBarLabelColors);
    display.setControlColors(rightLabel, display.getTheme().topBarLabelColors);
#ifdef BATTERY_ENABLED
    display.setControlColors(battery, display.getTheme().batteryIconColors);
#endif
    display.setControlColors(performBox, &display.getTheme().red);
    barLabelData.line.color = display.getTheme().topBarLabelColors[2];
    display.setControlColors(bar, display.getTheme().topBarLabelColors);
}

void TitleBar::show(bool showRightLabel)
{
    display.setControlShow(titleLabel);
    display.refreshControl(titleLabel);

    if (showRightLabel)
    {
        display.setControlShow(rightLabel);
        display.refreshControl(rightLabel);
    }
    else
    {
        display.setControlHide(rightLabel);
    }

#ifdef BATTERY_ENABLED
    display.setControlShow(battery);
    display.refreshControl(battery);
#endif

    display.setControlShow(bar);
    display.refreshControl(bar);

    visible = true;
}

void TitleBar::setTitle(const char* text)
{
    display.setControlText(titleLabel, text);
    display.refreshControl(titleLabel);
}

void TitleBar::setRightLabel(const char* text)
{
    display.setControlText(rightLabel, text);
    display.refreshControl(rightLabel);
}

void TitleBar::showCurrentInstrumentOnRightLabel()
{
    static char actualInstrName[SAMPLE_NAME_SIZE + 1];
    actualInstrName[0] = 0;

    uint8_t idx = mtProject.values.lastUsedInstrument;

    if (idx >= SYNTH_INSTRUMENTS_START_IDX)
    {
#if GUI_HANDLE_SYNTH
        assert(synth);
        assert((idx - SYNTH_INSTRUMENTS_START_IDX) < kSynthInstrumentsCount);
        strncpy(actualInstrName,
                synth->getSynthName((idx - SYNTH_INSTRUMENTS_START_IDX)).c_str(),
                SAMPLE_NAME_SIZE);
        actualInstrName[SAMPLE_NAME_SIZE] = 0;
#endif
    }
    else if (isMidiInstrument(idx)) // MPDJ: slot type, numbered 1-64 like every slot
    {
        snprintf(actualInstrName,
                 SAMPLE_NAME_SIZE,
                 "%d. MIDI Channel %d",
                 idx + 1,
                 instrumentIdxToMidiChannel(idx));
    }
    else
    {
#ifdef SHOW_EMPTY_SLOT_LABEL
        if (mtProject.instrument[idx].isActive)
        {
#endif
            snprintf(actualInstrName,
                     SAMPLE_NAME_SIZE,
                     "%d. %s",
                     idx + 1,
                     mtProject.instrument[idx].sample.file_name);
#ifdef SHOW_EMPTY_SLOT_LABEL
        }
        else
        {
            snprintf(actualInstrName, SAMPLE_NAME_SIZE, "%d. Empty slot", idx + 1);
        }
#endif
    }

    display.setControlText(rightLabel, actualInstrName);
    display.setControlShow(rightLabel);
    display.refreshControl(rightLabel);
}

void TitleBar::setBatteryValue([[maybe_unused]] int valuePercent, [[maybe_unused]] bool charging)
{
#ifdef BATTERY_ENABLED
    if (charging)
    {
        display.setAddControlStyle(battery, controlStyleTransformImg);
    }
    else
    {
        display.setRemoveControlStyle(battery, controlStyleTransformImg);
    }

    display.setControlValue(battery, valuePercent);
    display.refreshControl(battery);
#endif
}

void TitleBar::showRightLabel(bool show)
{
    if (show)
    {
        display.setControlShow(rightLabel);
    }
    else
    {
        display.setControlHide(rightLabel);
    }
    display.refreshControl(rightLabel);
}

void TitleBar::showBattery(bool show)
{
    if (show)
    {
        display.setControlShow(battery);
    }
    else
    {
        display.setControlHide(battery);
    }
    display.refreshControl(battery);
}

void TitleBar::hide()
{
    display.setControlHide(titleLabel);
    // display.refreshControl(titleLabel);

    display.setControlHide(rightLabel);
    // display.refreshControl(rightLabel);

#ifdef BATTERY_ENABLED
    display.setControlHide(battery);
    // display.refreshControl(battery);
#endif

    display.setControlHide(bar);
    display.refreshControl(bar);

    visible = false;
}

void TitleBar::refresh()
{
    if (refreshTimer < 300)
    {
        return;
    }

    refreshTimer = 0;

#if GUI_HANDLE_SYNTH
    Profiler::AudioProcessorUsage usage;
    Profiler::getInstance()->getAudioProcessingUsagePercent(usage, true);

    bool showBox = false;
    uint32_t boxColor = 0;

    if (usage.tempMaximum >= kPerformance1Treshold)
    {
        showBox = true;
        boxShowLoopCount = 0;

        if (usage.tempMaximum >= kPerformance3Treshold)
        {
            boxColor = display.getTheme().red;
        }
        else if (usage.tempMaximum >= kPerformance2Treshold)
        {
            boxColor = display.getTheme().orange;
        }
        else if (usage.tempMaximum >= kPerformance1Treshold)
        {
            boxColor = display.getTheme().yellow;
        }
    }

    if (showBox && boxLastColor != boxColor)
    {
        boxLastColor = boxColor;
        display.setControlColors(performBox, &boxLastColor);
        display.setControlShow(performBox);
        display.refreshControl(performBox);
    }
    else if (!showBox && boxLastColor)
    {
        if (boxShowLoopCount >= kBoxHideDelayLoopCount)
        {
            display.setControlHide(performBox);
            display.refreshControl(performBox);
            boxLastColor = 0;
        }
        else
        {
            boxShowLoopCount++;
        }
    }
#endif

#ifdef BATTERY_ENABLED
    if (!visible)
    {
        return;
    }

    bool state = batteryDriver->getState();
    int level = state ? batteryDriver->getLevel() : kBatteryNotPresent;
    bool charging = state ? batteryDriver->getCharging() : false;

    if (lastBatteryLevel != level || charging != lastBatteryCharging)
    {
        lastBatteryCharging = charging;
        lastBatteryLevel = level;
        setBatteryValue(lastBatteryLevel, lastBatteryCharging);
    }
#endif
}
