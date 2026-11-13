#include "mtSequencer.h"
#include "mpdjTracks.h"
#include "performanceEngine.h"
#include "performanceMode.h"

static char* getPerfTrackName(uint8_t track)
{
    char* name = trackNameRef(track); // MPDJ: tracks 1-8 / 9-16 are stored apart
    name[7] = 0;
    return name;
}

static bool isPerfTrackMuted(uint8_t track)
{
    return trackMuteRef(track) > 0;
}

static const uint16_t framesPlaces[12][4] = {
    { 11, 31, 62, 391 },          { 11 + 65 * 1, 31, 62, 391 },  { 11 + 65 * 2, 31, 62, 391 },
    { 11 + 65 * 3, 31, 62, 391 }, { 11 + 65 * 4, 31, 62, 391 },  { 11 + 65 * 5, 31, 62, 391 },
    { 11 + 65 * 6, 31, 62, 391 }, { 11 + 65 * 7, 31, 62, 391 },  { 11 + 65 * 8, 31, 62, 391 },
    { 11 + 65 * 9, 31, 62, 391 }, { 11 + 65 * 10, 31, 62, 391 }, { 11 + 65 * 11, 31, 62, 391 },
};

void cPerformanceMode::initDisplayControls()
{
    strControlProperties prop;
    frameData.placesCount = 12;
    frameData.startPlace = 0;
    frameData.places[0] = &framesPlaces[0][0];
    frameData.places[1] = &framesPlaces[1][0];
    frameData.places[2] = &framesPlaces[2][0];
    frameData.places[3] = &framesPlaces[3][0];
    frameData.places[4] = &framesPlaces[4][0];
    frameData.places[5] = &framesPlaces[5][0];
    frameData.places[6] = &framesPlaces[6][0];
    frameData.places[7] = &framesPlaces[7][0];
    frameData.places[8] = &framesPlaces[8][0];
    frameData.places[9] = &framesPlaces[9][0];
    frameData.places[10] = &framesPlaces[10][0];
    frameData.places[11] = &framesPlaces[11][0];
    prop.style = 0;
    prop.value = 0;
    prop.colors = nullptr;
    prop.data = &frameData;
    frameControl = display.createControl<cFrame>(&prop);

    strControlProperties prop2;
    for (uint8_t i = 0; i < 8; i++)
    {
        prop2.text = (char*)"";
        prop2.colors = display.getTheme().activeLabelsColors;
        prop2.style = (controlStyleCenterX | controlStyleCenterY | controlStyleFont3);
        prop2.x = (800 / 8) * i + (800 / 16);
        prop2.w = 800 / 8 - 6;
        prop2.y = 424;
        prop2.h = 55;
        prop2.value = 1;

        label[i] = display.createControl<cLabel>(&prop2);
    }

    prop2.text = nullptr;
    prop2.colors = nullptr;
    prop2.value = 255;
    prop2.style = controlStyleNoTransparency;
    prop2.x = 0;
    prop2.w = 800;
    prop2.y = 425;
    prop2.h = 54;
    bgLabel = display.createControl<cBgLabel>(&prop2);

    for (uint8_t i = 0; i < 12; i++)
    {
        prop.data = nullptr;
        prop.style = (controlStyleCenterX | controlStyleCenterY | controlStyleFont2 |
                      controlStyleVerticalText);
        prop.x = 42 + 65 * i;
        prop.y = 210 + 34;
        prop.colors = display.getTheme().perfFxNameTextLabelColors;
        textLabel[i] = display.createControl<cLabel>(&prop);

        multiLabelData[i].yForFxTextHole = 210;
        multiLabelData[i].textLinesCount = 5;
        multiLabelData[i].xOffset = 0;
        multiLabelData[i].yOffset = 16;
        multiLabelData[i].text[0] = &fxValuesText[i][0][0];
        multiLabelData[i].text[1] = &fxValuesText[i][1][0];
        multiLabelData[i].text[2] = &fxValuesText[i][2][0];
        multiLabelData[i].text[3] = &fxValuesText[i][3][0];
        multiLabelData[i].text[4] = &fxValuesText[i][4][0];

        prop.style = (controlStyleCenterX | controlStyleFont2 | controlStyleBackground |
                      (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0));
        prop.x = 42 + 65 * i;
        prop.y = 34;
        prop.colors = display.getTheme().perfValueNormalMLabelColors;
        prop.w = 62;
        prop.h = 174 + 210 + 5;
        prop.data = &multiLabelData[i];
        value1Label[i] = display.createControl<cMultiLabel>(&prop);
    }
}

void cPerformanceMode::showPerformanceFxes()
{
    titleBar->show();
    titleBar->setTitle("Performance FX");

    for (uint8_t i = 0; i < 8; i++)
    {
        if (isButtonActiveOnCurrentPage(i))
        {
            display.setControlText(label[i], getPerfTrackName(getTrackForButton(i)));
        }
        else
        {
            display.setControlText(label[i], (char*)"");
        }
    }

    //-------------------------------------
    for (uint8_t i = 0; i < 12; i++)
    {
        display.setControlText(textLabel[i], performance.getSlotFxName(i));
        display.setControlShow(textLabel[i]);
        display.refreshControl(textLabel[i]);

        showPerformaceValue(i);
    }

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlStyle2(label[i],
                                 controlStyleCenterX | controlStyleCenterY | controlStyleFont2);
        display.setControlShow(label[i]);
        display.refreshControl(label[i]);
    }

    hideEditFrame(performanceEditPlace);

    showTracksPatterns();
    showTracksState();

    display.setControlShow(bgLabel);
    display.refreshControl(bgLabel);

    display.synchronizeRefresh();
}

//====================================================================================================

void cPerformanceMode::showEditFrame(uint8_t place)
{
    display.setControlValue(frameControl, place);
    display.setControlColors(frameControl, display.getTheme().frameColors);

    display.setControlShow(frameControl);
    display.refreshControl(frameControl);
}

void cPerformanceMode::hideEditFrame(uint8_t place)
{
#ifdef TRACKER_MINI
    display.setControlColors(frameControl, display.getTheme().greenFrameColors);
    display.setControlValue(frameControl, place);
    display.setControlShow(frameControl);
    display.refreshControl(frameControl);
#else
    display.setControlHide(frameControl);
    display.refreshControl(frameControl);
#endif
}

void cPerformanceMode::showFxNames(uint8_t place)
{
    if (place < 12)
    {
        display.setControlText(textLabel[place], performance.getSlotFxName(place));
        display.setControlShow(textLabel[place]);
        display.refreshControl(textLabel[place]);
        return;
    }

    for (uint8_t i = 0; i < 12; i++)
    {
        display.setControlText(textLabel[i], performance.getSlotFxName(i));
        display.setControlShow(textLabel[i]);
        display.refreshControl(textLabel[i]);
    }
}

void cPerformanceMode::hideFxNames()
{
    for (uint8_t i = 0; i < 12; i++)
    {
        display.setControlHide(textLabel[i]);
    }

    display.refreshControl(textLabel[0]);
}

void cPerformanceMode::showTracksState(uint8_t shiftPressed)
{
    if (shiftPressed)
    {
        showMuteButtons(1);

        return;
    }

    for (uint8_t i = 0; i < 8; i++)
    {
        if (isButtonActiveOnCurrentPage(i))
        {
            uint8_t track = getTrackForButton(i);
            const uint32_t* ptrColors = display.getTheme().performActiveTrackLabelsColors;
            if (isPerfTrackMuted(track))
                ptrColors = display.getTheme().performMutedTrackLabelsColors;

            display.setControlText(label[i], getPerfTrackName(track));
            display.setControlColors(label[i], ptrColors);

            if (performance.getTrackState(track) == 1)
            {
                display.setControlStyle(
                  label[i],
                  controlStyleCenterX | controlStyleCenterY | controlStyleFont3 |
                    controlStyleBackground /*| controlStyleNoTransparency*/ /*controlStyleBorder*/);
            }
            else
            {
                display.setControlStyle(
                  label[i], controlStyleCenterX | controlStyleCenterY | controlStyleFont3);
            }
        }
        else
        {
            display.setControlText(label[i], (char*)"");
            display.setControlColors(label[i], display.getTheme().inactiveLabelsColors);
            display.setControlStyle(label[i],
                                    controlStyleCenterX | controlStyleCenterY | controlStyleFont3);
        }

        display.setControlShow(label[i]);
        display.refreshControl(label[i]);
    }

    display.synchronizeRefresh();
}

void cPerformanceMode::showTracksPatterns()
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (isButtonActiveOnCurrentPage(i))
        {
            uint8_t track = getTrackForButton(i);
            sprintf(&trackPaternText[i][0],
                    mtConfig.general.numberingMode == NumberingMode::kHexadecimal ? "Patt: %02X"
                                                                                  : "Patt: %d",
                    mtProject.perfTracksPatterns[track]);
            display.setControlText2(label[i], &trackPaternText[i][0]);
        }
        else
        {
            display.setControlText2(label[i], (char*)"");
        }

        display.setControlShow(label[i]);
        display.refreshControl(label[i]);
    }

    display.synchronizeRefresh();
}

//=================================================================================
//=================================================================================
void cPerformanceMode::showPerformaceValue(uint8_t place)
{
    if (place >= 12)
        return;

    multiLabelData[place].selected = performance.getSelectedRow(place) + 1;

    performance.fillSlotFxValue(place, 0, &fxValuesText[place][0][0]);
    performance.fillSlotFxValue(place, 1, &fxValuesText[place][1][0]);
    performance.fillSlotFxValue(place, 2, &fxValuesText[place][2][0]);
    performance.fillSlotFxValue(place, 3, &fxValuesText[place][3][0]);
    performance.fillSlotFxValue(place, 4, &fxValuesText[place][4][0]);

    if (performanceEditState)
    {
        display.setControlColors(value1Label[place], display.getTheme().perfValueEditMLabelColors);
    }
    else
    {
        display.setControlColors(value1Label[place],
                                 display.getTheme().perfValueNormalMLabelColors);
    }

    display.setControlShow(value1Label[place]);
    display.refreshControl(value1Label[place]);
}

void cPerformanceMode::showAllPerformanceValues()
{
    for (uint8_t place = 0; place < 12; place++)
    {
        showPerformaceValue(place);
    }

    display.synchronizeRefresh();
}

void cPerformanceMode::colorTracksLabel(uint8_t button, uint8_t state)
{
    if (!isButtonActiveOnCurrentPage(button))
    {
        return;
    }

    uint8_t track = getTrackForButton(button);
    if (state)
    {
        display.setControlColors(label[button], display.getTheme().redBlinkLabelColors);
    }
    else
    {
        const uint32_t* ptrColors = display.getTheme().performActiveTrackLabelsColors;
        if (isPerfTrackMuted(track))
        {
            ptrColors = display.getTheme().performMutedTrackLabelsColors;
        }

        display.setControlColors(label[button], ptrColors);
    }

    display.refreshControl(label[button]);
}

//##############################################################################################
//###############################        MUTE BUTTONS          #################################
//##############################################################################################
void cPerformanceMode::showMuteButtons(uint8_t state)
{
    if (state)
    {
        for (uint8_t i = 0; i < 8; i++)
        {
            if (isButtonActiveOnCurrentPage(i))
            {
                uint8_t track = getTrackForButton(i);
                bool muted = isPerfTrackMuted(track);
                const uint32_t* ptrColors = display.getTheme().performActiveTrackLabelsColors;
                if (muted)
                    ptrColors = display.getTheme().performMutedTrackLabelsColors;

                display.setControlColors(label[i], ptrColors);

                display.setControlText(label[i], getPerfTrackName(track));
                display.setControlText2(label[i], muted ? (char*)"Unmute" : (char*)"Mute");
            }
            else
            {
                display.setControlColors(label[i], display.getTheme().inactiveLabelsColors);
                display.setControlText(label[i], (char*)"");
                display.setControlText2(label[i], (char*)"");
            }

            display.setControlStyle(label[i],
                                    controlStyleCenterX | controlStyleCenterY | controlStyleFont3);

            display.setControlShow(label[i]);
            display.refreshControl(label[i]);
        }

        display.synchronizeRefresh();
    }
    else
    {
        showTracksPatterns();
        showTracksState();
    }
}
