#include "songEditor/songEditor.h"

#include "Platform.h"
#include "mtSequencer.h"
#include "string.h"

static const uint16_t framesPlaces[3][4] = {
    { (800 / 8) * 0 + 1, 31, 500 - 3, 391 },
    { (800 / 8) * 5 + 1, 31, 800 / 4 - 3, 391 },
    { (800 / 8) * 7 + 1, 31, 800 / 8 - 3, 391 },
};

void cSongEditor::initDisplayControls()
{
    strControlProperties prop;
    frameData.placesCount = 3;
    frameData.startPlace = 2;

    frameData.places[0] = &framesPlaces[0][0];
    frameData.places[1] = &framesPlaces[1][0];
    frameData.places[2] = &framesPlaces[2][0];
#ifdef PLATFORM_MPC
    static const uint16_t swingPlace[4] = { (800 / 8) * 2 + 1, 424, 800 / 8 - 3, 55 }; // label 3 = Swing
    frameData.places[3] = swingPlace;
    frameData.placesCount = 4;
#endif

    prop.style = 0;
    prop.value = 0;
    prop.colors = nullptr;
    prop.data = &frameData;
    frameControl = display.createControl<cFrame>(&prop);

    strControlProperties prop1;
    prop1.y = 29;
    prop1.w = 800 / 8 - 3;
    prop1.style = controlStyleValue_0_100 | controlStyleBackground |
                  (display.getTheme().barShadow > 0 ? controlStyleGradient : 0) |
                  (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0);
    prop1.h = 394;
    prop1.x = (800 / 8) * 7 + 1;
    barControl = display.createControl<cBar>(&prop1);

    doubleArrowRed.bitmaps[0].bitmapIndex = displayDoubleArrowL;
    doubleArrowRed.bitmaps[0].xValue = (800 / 8) * 5 + (800 / 16) - 10;
    doubleArrowRed.bitmaps[0].yValue = 445;
    doubleArrowRed.bitmaps[1].bitmapIndex = displayDoubleArrowR;
    doubleArrowRed.bitmaps[1].xValue = (800 / 8) * 6 + (800 / 16) + 10;
    doubleArrowRed.bitmaps[1].yValue = 445;

    doubleArrowRed.line.color = display.getTheme().red;
    doubleArrowRed.line.x1 = -90;
    doubleArrowRed.line.y1 = 50;
    doubleArrowRed.line.x2 = 90;
    doubleArrowRed.line.y2 = 50;

    labelDataRed.line.color = display.getTheme().red;
    labelDataRed.line.x1 = -40;
    labelDataRed.line.y1 = 50;
    labelDataRed.line.x2 = 40;
    labelDataRed.line.y2 = 50;

    strControlProperties prop2;
    prop2.text = (char*)"";
    prop2.colors = display.getTheme().activeButtonLabelsColors;

    prop2.w = 800 / 8 - 6;
    prop2.y = 424;
    prop2.h = 55;
    prop2.value = 1;

    for (uint8_t i = 0; i < 8; i++)
    {
        prop2.style = controlStyleCenterX | controlStyleCenterY | controlStyleFont3 |
                      ((display.getTheme().patternEditorBottomButtonsLines && (i >= 2 && i <= 5)
                          ? controlStyleSpecialFx
                          : 0));

        prop2.x = (800 / 8) * i + (800 / 16);
        if (i == 5)
        {
            prop2.x = (800 / 8) * i + (800 / 8);
        }

        label[i] = display.createControl<cLabel>(&prop2);
    }

    prop2.value = 191; // 191
    prop2.text = nullptr;
    prop2.colors = nullptr;
    prop2.style = controlStyleNoTransparency;
    prop2.x = 0;
    prop2.w = 800;
    prop2.y = 425;
    prop2.h = 54;
    bgLabel = display.createControl<cBgLabel>(&prop2);

    prop.data = nullptr;
    prop.x = 0 + 1;
    prop.y = 29;
    prop.w = ((800 / 8) * 7) - 3;
    prop.h = 394;
    prop.style =
      controlStyleBackground | controlStyleVerticalText |
      (mtConfig.general.numberingMode == NumberingMode::kHexadecimal ? controlStyleHexNumbers : 0) |
      (mtConfig.general.numberingMode == NumberingMode::kDecimalWithStepsFrom1
         ? controlStyleStartNumbersFrom1
         : 0) |
      (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0);

    songPlayerControl = display.createControl<cSongPlayer>(&prop);
}

void cSongEditor::refreshTempoField()
{
    if (tapTempoActive && tapTempoCount > 1)
    {
        memset(tapText, 0, sizeof(tapText));
        snprintf(tapText, sizeof(tapText), "TAP %d/4", tapTempoCount);

        display.setControlText(label[7], tapText);
    }
    else
    {
        if (sequencer.isSetInternalClock())
        {
            display.setControlText(label[7], "Tempo");
        }
        else
        {
            display.setControlText(label[7], "Ext Tempo");
        }
    }
    display.refreshControl(label[7]);
    showTempoValue();
}

void cSongEditor::refreshAddSlotLabel(bool isButtonPressed)
{
    if (isButtonPressed)
    {
        display.setControlText(label[3], "Add");
        display.setControlText2(label[3], "Empty Slot");
    }
    else
    {
        display.setControlText(label[3], "Add");
        display.setControlText2(label[3], "Slot");
    }
    display.refreshControl(label[3]);
}

void cSongEditor::showDefaultScreen()
{
    titleBar->show();
    titleBar->setTitle("Song");

    display.setControlStyle2(label[7],
                             controlStyleCenterX | controlStyleCenterY | controlStyleFont2);
#ifdef PLATFORM_MPC
    display.setControlData(label[2], nullptr); // Swing: a value, not an action
#else
    display.setControlData(label[2], &labelDataRed);
#endif
    display.setControlData(label[3], &labelDataRed);
    display.setControlData(label[4], &labelDataRed);
    display.setControlData(label[5], &doubleArrowRed);
    display.setAddControlStyle(label[5], controlStyleShowBitmap);

    playButtonsState = 0;

    display.setControlText(label[0], "Play");
    display.setControlText2(label[0], "Pattern");

    display.setControlText(label[1], "Play");
    display.setControlText2(label[1], "Song");

#ifdef PLATFORM_MPC
    display.setControlText(label[2], "Swing");
    mpdjShowSwing();
#else
    display.setControlText(label[2], "Undo");
#endif

    if (buttons->isButtonPressed(interfaceButtonShift))
    {
        display.setControlText(label[3], "Add");
        display.setControlText2(label[3], "Empty Slot");
    }
    else
    {
        display.setControlText(label[3], "Add");
        display.setControlText2(label[3], "Slot");
    }

    display.setControlText(label[4], "Delete");
    display.setControlText2(label[4], "Slot");

    display.setControlText(label[5], "Pattern");
    display.setControlText2(label[5], "");

    display.setControlText(label[6], "");
    display.setControlText2(label[6], "");

    display.setControlText(label[7], "Tempo");

    if (sequencer.isSetInternalClock())
    {
        display.setControlColors(label[7], display.getTheme().activeLabelsColors);
    }
    else
    {
        display.setControlColors(label[7], display.getTheme().inactiveLabelsColors);
    }

    showTempoValue();

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlValue(label[i], 1);
        display.setControlShow(label[i]);
        display.refreshControl(label[i]);
    }

#ifndef PLATFORM_MPC
    display.setControlValue(label[2], 0);
#endif
    display.setControlValue(label[5], 0);

    display.setControlShow(bgLabel);
    display.refreshControl(bgLabel);

    display.setControlShow(songPlayerControl);
    display.refreshControl(songPlayerControl);

    display.synchronizeRefresh();
}

//==============================================================================================================
void cSongEditor::refreshSongPlayerControl()
{
    display.refreshControl(songPlayerControl);
    display.synchronizeRefresh();
}

void cSongEditor::refreshSongPlayerSelectedSlot()
{
    display.setControlValue(songPlayerControl, selectedSongPosition);
    display.refreshControl(songPlayerControl);
    display.synchronizeRefresh();
}

void cSongEditor::refreshButtonsLabels()
{
    Sequencer::enSeqState seqState = sequencer.getSeqState();
    if (seqState == Sequencer::SEQ_STATE_STOP)
    {
        if (buttons->isButtonPressed(interfaceButtonShift) && selectedPlace <= 0 &&
            songPlayerData.selection.trackSelectionLength == 1 &&
            songPlayerData.selection.patternSelectionLength == 1)
        {
            if (playButtonsState != 1)
            {
                display.setControlText(label[0], "Preview");
                display.setControlText2(label[0], "Track");

                display.setControlText(label[1], "Play");
                display.setControlText2(label[1], "Song");

                display.refreshControl(label[0]);
                display.refreshControl(label[1]);
                playButtonsState = 1;
            }
        }
        else
        {
            if (playButtonsState != 0)
            {
                display.setControlText(label[0], "Play");
                display.setControlText2(label[0], "Pattern");

                display.setControlText(label[1], "Play");
                display.setControlText2(label[1], "Song");

                display.refreshControl(label[0]);
                display.refreshControl(label[1]);
                playButtonsState = 0;
            }
        }
    }
    else if (seqState == Sequencer::SEQ_STATE_PLAY_SONG)
    {
        if (playButtonsState != 2)
        {
            display.setControlText(label[0], "Loop");
            display.setControlText2(label[0], "Pattern");
            display.setControlText(label[1], "Stop");
            display.setControlText2(label[1], "");
            display.refreshControl(label[0]);
            display.refreshControl(label[1]);
            playButtonsState = 2;
        }
    }
    else if (seqState == Sequencer::SEQ_STATE_PLAY_PATTERN)
    {
        if (playButtonsState != 3)
        {
            display.setControlText(label[0], "Continue");
            display.setControlText2(label[0], "Song");
            display.setControlText(label[1], "Stop");
            display.setControlText2(label[1], "");
            display.refreshControl(label[0]);
            display.refreshControl(label[1]);
            playButtonsState = 3;
        }
    }
    else if (seqState == Sequencer::SEQ_STATE_PLAY_PERFORMANCE)
    {
        if (sequencer.isSongMode())
        {
            // ktoś wcisnął "Continue Song" i teraz ma jeszcze krótką chwilę,
            // żeby zmienić zdanie
            if (playButtonsState != 4)
            {
                display.setControlText(label[0], "Loop");
                display.setControlText2(label[0], "Performance");
                display.setControlText(label[1], "Stop");
                display.setControlText2(label[1], "");
                display.refreshControl(label[0]);
                display.refreshControl(label[1]);
                playButtonsState = 4;
            }
        }
        else
        {
            if (playButtonsState != 3)
            {
                display.setControlText(label[0], "Continue");
                display.setControlText2(label[0], "Song");
                display.setControlText(label[1], "Stop");
                display.setControlText2(label[1], "");
                display.refreshControl(label[0]);
                display.refreshControl(label[1]);
                playButtonsState = 3;
            }
        }
    }
    else
    {
        if (playButtonsState != 5)
        {
            display.setControlText(label[0], "Stop");
            display.setControlText2(label[0], "");
            display.setControlText(label[1], "Stop");
            display.setControlText2(label[1], "");
            display.refreshControl(label[0]);
            display.refreshControl(label[1]);
            playButtonsState = 5;
        }
    }
}

//==============================================================================================================

void cSongEditor::activateLabelsBorder()
{
    if (selectedPlace > frameData.placesCount - 1)
        return;

    if (selectedPlace == 2 || selectedPlace == 3) // tempo, swing (MPDJ)
    {
        display.setControlValue(frameControl, selectedPlace);
        display.setControlShow(frameControl);
        display.refreshControl(frameControl);
        songPlayerData.isListActive = 0;
        songPlayerData.selection.isActive = 0;
        display.refreshControl(songPlayerControl);
        display.synchronizeRefresh();
        return;
    }
    else if (selectedPlace == 1) // lista
    {
        songPlayerData.isListActive = 1;
        songPlayerData.selection.isActive = 0;
        display.refreshControl(songPlayerControl);
    }
    else // tracks matrix
    {
        songPlayerData.isListActive = 0;
        songPlayerData.selection.isActive = 1;
        display.refreshControl(songPlayerControl);
    }

    display.setControlHide(frameControl);
    display.refreshControl(frameControl);
    display.synchronizeRefresh();
}

void cSongEditor::showTempoValue()
{
    if (sequencer.isSetInternalClock())
    {
        sprintf(globalTempoVal, "%.1f BPM", mtProject.values.globalTempo);
    }
    else
    {
        float externalTempo = sequencer.getExternalTempo();

        if (externalTempo == Sequencer::kUnknownExternalTempo)
        {
            sprintf(globalTempoVal, "NO CLK");
        }
        else
        {
            sprintf(globalTempoVal, "%.0f BPM", externalTempo);
        }
    }

    display.setControlText2(label[7], globalTempoVal);
    display.refreshControl(label[7]);

    display.setControlShow(barControl);
    display.setControlValue(barControl,
                            (mtProject.values.globalTempo * 100) / Sequencer::MAX_TEMPO);
    display.refreshControl(barControl);
}

void cSongEditor::showIcon(icon_t iconType, uint8_t position)
{
    patternsList.icon.useSpecialIcons = 1;
    patternsList.icon.iconPositionInList = position;
    patternsList.icon.iconNum = displayPlayIcon + iconType;

    // display.setControlValue(songPlayerControl, selectedPattern);
    display.refreshControl(songPlayerControl);
}

void cSongEditor::hideIcon()
{
    patternsList.icon.useSpecialIcons = 0;
    patternsList.icon.iconNum = displayPlayIcon;
    patternsList.icon.iconPositionInList = 0;

    // display.setControlValue(songPlayerControl, selectedPattern);
    display.refreshControl(songPlayerControl);
}
