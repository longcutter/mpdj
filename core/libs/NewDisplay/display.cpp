#include <string.h>

#include "FT812.h"

#include "DisplayDriver.h"
#include "Platform.h"
#include "Themes.h"
#include "TrackerImpl.h"
#include "display.h"
#include "elapsedMillis.h"

#include "polyassert.h"

// FONTS
#include "RobotoMono-Bold_17_L4.h"
#include "RobotoMono-Light_14_L4.h"
#include "RobotoMono-Light_17_L4.h"
#include "RobotoMono-Regular_12_L4.h"
#include "RobotoMono-Regular_14_L4.h"

#include "LiberationSans-Bold_14_L4.h"
#include "LiberationSans-Bold_17_L4.h"
#include "LiberationSans-Regular_12_L4.h"
#include "LiberationSans-Regular_14_L4.h"
#include "LiberationSans-Regular_17_L4.h"

// BITMAPS
#ifndef TRACKER_MINI
#include "images/poly_logo_160x172_L8.h"
#else
#include "images/poly_logo_172x172_L8.h"
#endif
#include "CompressionRLE.h"
#include "images/arrowIcons.h"
#include "images/battery_28x13_L4.h"
#include "images/battery_ch_28x19_L4.h"
#include "images/songIcons.h"
#include "log.h"
#include <cr_section_macros.h>

#ifdef FEATURE_SYNTH
#include "images/acd_mini_nomark_80x29_L2.h"
#include "images/fat_mini_nomark_80x29_L2.h"
#include "images/perc_mini_nomark_80x29_L2.h"
#include "images/phz_mini_nomark_80x29_L2.h"
#include "images/vap_mini_nomark_80x29_L2.h"
#include "images/wtfm_mini_nomark_80x29_L2.h"

#include "images/acd_med_108x40_L2.h"
#include "images/fat_med_108x40_L2.h"
#include "images/perc_med_108x40_L2.h"
#include "images/phz_med_108x40_L2.h"
#include "images/vap_med_108x40_L2.h"
#include "images/wtfm_med_108x40_L2.h"

#include "images/synth_env_18x14_L4.h"
#include "images/synth_filter_18x13_L4.h"
#include "images/synth_gear_18x18_L4.h"
#include "images/synth_lfo_20x13_L4.h"
#include "images/synth_macro_14x14_L4.h"
#include "images/synth_mod_22x12_L4.h"
#include "images/synth_osc_20x12_L4.h"

#include "images/synth_FX_20x18_L4.h"
#include "images/synth_cymbal_20x16_L4.h"
#include "images/synth_hihat_20x16_L4.h"
#include "images/synth_kick_16x19_L4.h"
#include "images/synth_mixer_16x14_L4.h"
#include "images/synth_percussion_16x16_L4.h"
#include "images/synth_snare_20x16_L4.h"
#include "images/synth_tom_20x16_L4.h"

#include "images/dirt_med_108x40_L2.h"
#include "images/dirt_mini_nomark_80x28_L2.h"
#include "images/synth_shaper_18x18_L4.h"
#include "images/chip_med_108x40_L2.h"
#include "images/chip_mini_nomark_80x28_L2.h"

#endif

static constexpr int kMaxFontBufferSize = 22396;
__NOINIT(EXTERNAL_RAM) uint8_t poly_logo_160x172_l8[27520];
__NOINIT(EXTERNAL_RAM) uint8_t decompressedFontsCommonBuffer[kMaxFontBufferSize];

const uint8_t displayFontCount = 5;

// clang-format off

const CompressedFontData fontsData2[displayFontCount] =
{
    {
      LiberationSans_Regular_14_L4_compressed,
      sizeof(LiberationSans_Regular_14_L4_compressed) / sizeof(LiberationSans_Regular_14_L4_compressed[0])
    },
    {
      LiberationSans_Regular_17_L4_compressed,
      sizeof(LiberationSans_Regular_17_L4_compressed) / sizeof(LiberationSans_Regular_17_L4_compressed[0])
    },
    {
      LiberationSans_Bold_14_L4_compressed,
      sizeof(LiberationSans_Bold_14_L4_compressed) / sizeof(LiberationSans_Bold_14_L4_compressed[0])
    },
    {
      LiberationSans_Bold_17_L4_compressed,
      sizeof(LiberationSans_Bold_17_L4_compressed) / sizeof(LiberationSans_Bold_17_L4_compressed[0])
    },
    {
      LiberationSans_Regular_12_L4_compressed,
      sizeof(LiberationSans_Regular_12_L4_compressed) / sizeof(LiberationSans_Regular_12_L4_compressed[0])
    },
};

const CompressedFontData fontsData1[displayFontCount] =
{
    {
      Roboto_Mono_Light_14_L4_compressed,
      sizeof(Roboto_Mono_Light_14_L4_compressed) / sizeof(Roboto_Mono_Light_14_L4_compressed[0])
    },
    {
      Roboto_Mono_Light_17_L4_compressed,
      sizeof(Roboto_Mono_Light_17_L4_compressed) / sizeof(Roboto_Mono_Light_17_L4_compressed[0])
    },
    {
      Roboto_Mono_Regular_14_L4_compressed,
      sizeof(Roboto_Mono_Regular_14_L4_compressed) / sizeof(Roboto_Mono_Regular_14_L4_compressed[0])
    },
    {
      Roboto_Mono_Bold_17_L4_compressed,
      sizeof(Roboto_Mono_Bold_17_L4_compressed) / sizeof(Roboto_Mono_Bold_17_L4_compressed[0])
    },
    {
      Roboto_Mono_Regular_12_L4_compressed,
      sizeof(Roboto_Mono_Regular_12_L4_compressed) / sizeof(Roboto_Mono_Regular_12_L4_compressed[0])
    },
};

// clang-format on
__NOINIT(EXTERNAL_RAM)
strFont fonts[displayFontCount];

#ifdef PLATFORM_MPC
extern const uint8_t mpdj_logo_320x162_l4[25920]; // platforms/mpc/source/MpdjLogo.cpp
#endif

const strBitmap bitmaps[displayBitmapsCount] = {
#if defined(PLATFORM_MPC)
    // MPDJ: the MPDJ logo on the boot / start screen instead of Polyend's
    // (fits the logo slot: 25,920 bytes from 80,000, the next bitmap is at 110,000)
    { mpdj_logo_320x162_l4, 80000, 25920, 320, 162, L4, 160 },
#elif !defined(TRACKER_MINI)
    {
      poly_logo_160x172_l8,
      80000,
      27520,
      160,
      172,
      L8,
      160,
    },
#else
    {
      poly_logo_172x172_l8,
      80000,
      29584,
      172,
      172,
      L8,
      172,
    },
#endif

    { playIcon20x20, 110000, 200, 20, 20, L4, 10 },
    { loopIcon20x20, 110500, 200, 20, 20, L4, 10 },

    { doubleArrowL, 111000, 135, 18, 15, L4, 9 },
    { doubleArrowR, 111150, 135, 18, 15, L4, 9 },
    { arrowU, 111300, 64, 16, 8, L4, 8 },
    { arrowD, 111400, 64, 16, 8, L4, 8 },
    { battery_28x13_L4, 111500, 182, 28, 13, L4, 14 },
    { battery_ch_28x19_L4, 111700, 266, 28, 19, L4, 14 },

#ifdef FEATURE_SYNTH
    { acd_mini_nomark_80x29_L2, 112000, 580, 80, 29, L2, 20 },
    { fat_mini_nomark_80x29_L2, 112600, 580, 80, 29, L2, 20 },
    { vap_mini_nomark_80x29_L2, 113200, 580, 80, 29, L2, 20 },
    { wtfm_mini_nomark_80x29_L2, 113800, 580, 80, 29, L2, 20 },

    { acd_med_108x40_L2, 114400, 1080, 108, 40, L2, 27 },
    { fat_med_108x40_L2, 115500, 1080, 108, 40, L2, 27 },
    { vap_med_108x40_L2, 116600, 1080, 108, 40, L2, 27 },
    { wtfm_med_108x40_L2, 117700, 1080, 108, 40, L2, 27 },

    { synth_env_18x14_L4, 118800, 126, 18, 14, L4, 9 },
    { synth_filter_18x13_L4, 119000, 117, 18, 13, L4, 9 },
    { synth_gear_18x18_L4, 119200, 162, 18, 18, L4, 9 },
    { synth_lfo_20x13_L4, 119400, 130, 20, 13, L4, 10 },
    { synth_macro_14x14_L4, 119600, 98, 14, 14, L4, 7 },
    { synth_mod_22x12_L4, 119800, 132, 22, 12, L4, 11 },
    { synth_osc_20x12_L4, 120000, 120, 20, 12, L4, 10 },

    { perc_mini_nomark_80x29_L2, 120200, 580, 80, 29, L2, 20 },
    { phz_mini_nomark_80x29_L2, 120800, 580, 80, 29, L2, 20 },
    { perc_med_108x40_L2, 121400, 1080, 108, 40, L2, 27 },
    { phz_med_108x40_L2, 122500, 1080, 108, 40, L2, 27 },

    { synth_FX_20x18_L4, 123600, 180, 20, 18, L4, 10 },
    { synth_cymbal_20x16_L4, 123800, 160, 20, 16, L4, 10 },
    { synth_hihat_20x16_L4, 124000, 160, 20, 16, L4, 10 },
    { synth_kick_16x19_L4, 124200, 152, 16, 19, L4, 8 },
    { synth_mixer_16x14_L4, 124400, 112, 16, 14, L4, 8 },
    { synth_percussion_16x16_L4, 124600, 128, 16, 16, L4, 8 },
    { synth_snare_20x16_L4, 124800, 160, 20, 16, L4, 10 },
    { synth_tom_20x16_L4, 125000, 160, 20, 16, L4, 10 },

    { dirt_mini_nomark_80x28_L2, 125200, 560, 80, 28, L2, 20 },
    { dirt_med_108x40_L2, 125800, 1080, 108, 40, L2, 27 },
    { synth_shaper_18x18_L4, 126900, 162, 18, 18, L4, 9 },
    { chip_mini_nomark_80x28_L2, 127100, 560, 80, 28, L2, 20 },
    { chip_med_108x40_L2, 127700, 1080, 108, 40, L2, 27 },

#endif
};

uint32_t ImageViewerJpegAddress = 350000;

strGetProps getProps;
cDisplay display;
FileSystem::FileHandle imgFile;

cDisplay::cDisplay()
{
    theme = &themeDefault;
}

void cDisplay::loadFont(const CompressedFontData* fontsData, uint8_t fontCount)
{
    displayDriver->beginCoProList();
    API_CMD_DLSTART();
    API_CLEAR_COLOR(0x000000);
    API_CLEAR(1, 1, 1);
    API_DISPLAY();
    displayDriver->endCoProList();

    uint16_t char_num;
    for (uint8_t i = 0; i < fontCount; i++)
    {
        uint8_t* decompressedFontData = decompressFont(&fontsData[i]);

        char_num = 0;
        for (int cha = 0; cha < 128; cha++)
        {
            if (decompressedFontData[cha])
            {
                char_num++;
            }
        }

        memcpy(fonts[i].data, decompressedFontData, 148);
        fonts[i].format = decompressedFontData[128];
        fonts[i].linestride = decompressedFontData[132];
        fonts[i].width = decompressedFontData[136];
        fonts[i].height = decompressedFontData[140];

        fonts[i].size = char_num * fonts[i].linestride * fonts[i].height + 148;
        fonts[i].handle = i + 1;

        fonts[i].address = (i == 0 ? 0 : fonts[i - 1].address + fonts[i - 1].size);
        int rest = fonts[i].address % 4;
        fonts[i].address = fonts[i].address + (4 - rest);

        fonts[i].source = fonts[i].address + 148;

        displayDriver->writeFontRAMG(decompressedFontData, fonts[i].size, fonts[i].address);
    }

    displayDriver->beginCoProList();
    API_CMD_DLSTART();

    API_CLEAR_COLOR(0x000000);
    API_CLEAR(1, 1, 1);

    uint8_t first_char;
    for (uint8_t i = 0; i < displayFontCount; i++)
    {
        first_char = 0;

        while (!fonts[i].data[first_char])
            first_char++;

        API_BITMAP_HANDLE(fonts[i].handle);
        API_BITMAP_SOURCE(fonts[i].source);
        API_BITMAP_LAYOUT(L4, fonts[i].linestride, fonts[i].height);
        API_BITMAP_SIZE(NEAREST, BORDER, BORDER, fonts[i].width, fonts[i].height);
        API_CMD_SETFONT2(fonts[i].handle, fonts[i].address, first_char);
    }

    API_DISPLAY();
    API_CMD_SWAP();
    displayDriver->endCoProList();
}

void cDisplay::begin(DisplayDriver* displayDriver, Timer* timer)
{
    this->displayDriver = displayDriver;
    cDisplayControl::setDisplayDriver(displayDriver);

    this->timer = timer;
    // ####################################
#if !defined(TRACKER_MINI) && !defined(PLATFORM_MPC)
    decompress(poly_logo_160x172_l8_compressed,
               sizeof(poly_logo_160x172_l8_compressed) / sizeof(poly_logo_160x172_l8_compressed[0]),
               poly_logo_160x172_l8);
#endif

    for (uint8_t i = 0; i < displayBitmapsCount; i++)
    {
        displayDriver->writeDataRAMG(bitmaps[i].data, bitmaps[i].source, bitmaps[i].address);
    }

    loadFont(fontsData1, displayFontCount);

    // ####################################

    // displayDriver->beginCoProListNoCheck();
    displayDriver->beginCoProList();

    API_CMD_DLSTART();

    API_CLEAR_COLOR(0x000000);
    API_CLEAR(1, 1, 1);

    API_VERTEX_FORMAT(0);

    constexpr int bitmap_index = displayPolyLogoBitmap;

    API_SAVE_CONTEXT();

    API_BITMAP_HANDLE(0);
    API_BITMAP_SOURCE(bitmaps[bitmap_index].address);
    API_BITMAP_LAYOUT(
      bitmaps[bitmap_index].format, bitmaps[bitmap_index].linestride, bitmaps[bitmap_index].height);
    API_BITMAP_SIZE(
      NEAREST, BORDER, BORDER, bitmaps[bitmap_index].width, bitmaps[bitmap_index].height);

    API_BEGIN(BITMAPS);
    API_VERTEX2F((800 / 2) - (bitmaps[bitmap_index].width / 2), 100);
    API_END();

    API_RESTORE_CONTEXT();

    API_DISPLAY();
    API_CMD_SWAP();

    displayDriver->endCoProList();
    displayDriver->awaitCoProEmpty();

    stopAppend = false;
    synchQueuePosition = 0;
    refreshQueueTop = 0;
    refreshQueueBott = 0;
    updateStep = 0;
    actualUpdatingStage = 0;
    freshControl = true;
    setTheme(0);
}

//=================================================================================================
//=================================================================================================

// elapsedMillis printTajner;

void cDisplay::update()
{
    if (!state)
        return;

    if (updateDisplaySettings)
    {
        updateDisplaySettings = 0;

        if (backlightBrightness != lastBacklightBrightness)
        {
            lastBacklightBrightness = backlightBrightness;
            displayDriver->setBrightness(backlightBrightness);
        }
        if (rotateValue != lastRotateValue)
        {
            lastRotateValue = rotateValue;
            displayDriver->setRotation(rotateValue);
        }
    }

    if (screenShotscheduled && timer->millis() > screenShotTimer)
    {
        screenShotscheduled = false;
        processDisplayUpdateImmediate();
        doScreenShot();
    }

    if (loadImage) // todo
    {
        FileSystem* fileSystem = TrackerImpl::getInstance()->getPlatform()->getFileSystem();

        if (!displayDriver->isCoProEmpty())
            return;

        if (img.status == 1)
        {
            if (fileSystem->getFileAvailable(imgFile))
            {
                int32_t readBytes = 0;
                uint8_t currentBuffer[imgBufforSize];
                readBytes = fileSystem->readFile(currentBuffer, imgBufforSize, imgFile);

                if (readBytes < 0)
                {
                    fileSystem->closeFile(imgFile, false);
                    img.status = 0;
                    img.loadProgress = 0;
                    loadImage = 0;
                    return;
                }

                if (img.loadProgress == 0)
                {
                    displayDriver->beginCoProListNoCheck();
                    API_CMD_LOADIMAGE(img.address, OPT_NODL);
                    displayDriver->endCoProList();
                }

                if (readBytes < imgBufforSize)
                {
                    // eof
                    displayDriver->writeDataToCMD(currentBuffer, readBytes);
                    // API_LIB_WriteDataRAMG(currentBuffer, readBytes, img.address +
                    // img.loadProgress*imgBufforSize);
                    fileSystem->closeFile(imgFile, false);
                    img.status = 2;
                }
                else
                {
                    displayDriver->writeDataToCMD(currentBuffer, imgBufforSize);
                    // API_LIB_WriteDataRAMG(currentBuffer, imgBufforSize, img.address +
                    // img.loadProgress*imgBufforSize);
                }
                // API_LIB_WriteDataToCMD(currentBuffer,imgBufforSize);
                // API_CMD_LOADIMAGE(img.address, OPT_NODL);

                img.loadProgress++;

                return;
            }
            else
            {
                fileSystem->closeFile(imgFile, false);
                img.status = 0;
                img.loadProgress = 0;
                loadImage = 0;
            }
        }
        else if (img.status == 2)
        {
            displayDriver->beginCoProList();
            API_CMD_GETPROPS(0, 0, 0); // 3 dummy 32-bit values
            displayDriver->endCoProList();
            displayDriver->awaitCoProEmpty();

            displayDriver->getImgProps(getProps.End_Address, getProps.Width, getProps.Height);

            img.status = 3;
            loadImage = 0;
        }
    }

    //=================================================================================================
    //=================================================================================================

    if (!displayDriver->isCoProEmpty())
    {
        return;
    }

    switch (updateStep)
    {
        case 0: step1(); break; // jesli jakas kontrolka w kolejce odswiezania, odswieza ja
        case 1: step2(); break; // zapisz DL z porzedniego kroku do ramu ukladu graficznego
        case 2: step3(); break; // oswiez cały ekran
        default: updateStep = 0; break;
    }
}

void cDisplay::step1()
{
    // check if refresh queue is not empty
    if (refreshQueueTop == refreshQueueBott)
    {
        if (stopAppend)
        {
            // if refresh queue is empty but 'stopAppend' flag is still set, process screen
            // update
            stopAppend = false;
            updateStep = 2;
        }
        return;
    }
    if (refreshQueue[refreshQueueBott] < 0 || refreshQueue[refreshQueueBott] >= controlsCount ||
        controlsTable[refreshQueue[refreshQueueBott]] == nullptr)
    {
        return;
    }

    if (actualUpdatingStage > 0 && !validateCurrentUpdateData())
    {
        return;
    }

    displayDriver->beginCoProListNoCheck();
    API_CMD_DLSTART();

    switch (actualUpdatingStage)
    {
        case 0:
        {
            actualUpdatingIndex = refreshQueue[refreshQueueBott];
            actualUpdating = controlsTable[actualUpdatingIndex];
            actualUpdating->update();
            break;
        }
        case 1:
        {
            actualUpdating->update2();
            break;
        }
        case 2:
        {
            actualUpdating->update3();
            break;
        }
        case 3:
        {
            actualUpdating->update4();
            break;
        }
        case 4:
        {
            actualUpdating->update5();
            break;
        }
        default: break;
    }

    displayDriver->endCoProList();

    updateStep = 1;
}

void cDisplay::step2()
{
    if (!validateCurrentUpdateData())
    {
        return;
    }
    // ------------
    uint32_t ramAddress = controlsRamStartAddress + (actualUpdatingIndex * controlsRamAddressStep);
    uint32_t dlSize = display.getDisplayDriver()->getDisplayListOffset();

    if (actualUpdatingStage == 0)
    {
        actualUpdating->ramSize = 0;
    }

    displayDriver->beginCoProListNoCheck();
    API_CMD_MEMCPY(ramAddress + actualUpdating->ramSize, RAM_DL, dlSize);
    displayDriver->endCoProList();

    actualUpdating->ramSize += dlSize;

    actualUpdatingStage++;

    if (actualUpdatingStage < actualUpdating->stagesCount)
    {
        updateStep = 0; // jesli obslugiwana kontrolka potrzebuje odswiezenia wiekszej ilosci blokow
        // step1();
        return;
    }

    // ------------

    if (actualUpdating->highPrioRefresh) // high priority control - odswieza tylko ją
    {
        actualUpdating->refresh = 1;
        actualUpdating = nullptr;
        actualUpdatingIndex = -1;
        actualUpdatingStage = 0;
        updateStep = 2;
        // step2();
        return;
    }

    // przesun kolejke odswieznania
    refreshQueueBott++;
    if (refreshQueueBott >= controlsRefreshQueueSize)
    {
        refreshQueueBott = 0;
    }

    // jesli bottom kolejki dogonil zapisana pozycje synch refreschu
    if (stopAppend && synchQueuePosition == refreshQueueBott)
    {
        stopAppend = false;
    }

    // jesli kontrolka jest animowana i potzebuje sie automatycznie dalej odswiezac
    // dodawana jest automatycznie na koniec kolejki
    if (actualUpdating->autoRefresh && (actualUpdating->style & controlStyleShow) != 0)
    {
        refreshControl(actualUpdatingIndex);
    }

    actualUpdating->refresh = 1;
    actualUpdating = nullptr;
    actualUpdatingIndex = -1;
    actualUpdatingStage = 0;

    updateStep = 2;
    // step3();
}

void cDisplay::step3()
{
    if (stopAppend)
    {
        updateStep = 0;
        step1();
        return;
    }

    displayDriver->beginCoProListNoCheck();
    API_CMD_DLSTART();

    API_CLEAR_COLOR(theme->backgroundColor);
    API_CLEAR(1, 1, 1);

    API_VERTEX_FORMAT(0);

    // wczytaj elementy w odwrotnej kolejnosci niz w jakiej byly tworzone
    for (int8_t i = controlsCount - 1; i >= 0; i--)
    {
        cDisplayControl* p = controlsTable[i];
        if (p == nullptr)
        {
            continue;
        }
        if ((p->style & controlStyleShow) && p->refresh)
        {
            uint32_t controlMemoryUsed = p->ramSize;
            if (controlMemoryUsed + memoryUsed < 8172)
            {
                memoryUsed += controlMemoryUsed;

                uint32_t controlRamAddress = controlsRamStartAddress + (i * controlsRamAddressStep);
                API_CMD_APPEND(controlRamAddress, controlMemoryUsed);
            }
            else
            {
                break;
            }
        }
    }

    API_DISPLAY();
    API_CMD_SWAP();

    displayDriver->endCoProList();

#if DEBUG
    if (memoryUsed > 0)
    {
        lastMemoryUsed = memoryUsed;
    }
#endif

    memoryUsed = 0;
    updateStep = 0;
}

bool cDisplay::validateCurrentUpdateData()
{
    if (actualUpdatingIndex < 0 || actualUpdatingIndex >= controlsCount ||
        actualUpdating == nullptr || actualUpdatingStage >= actualUpdating->stagesCount)
    {
        // safety check
        // if something is wrong with current updating control, for example it was deleted
        // between processing steps, then move to next control

        if (refreshQueueTop != refreshQueueBott)
        {
            refreshQueueBott++;
            if (refreshQueueBott >= controlsRefreshQueueSize)
            {
                refreshQueueBott = 0;
            }
        }
        updateStep = 0;
        actualUpdatingStage = 0;
        return false;
    }
    return true;
}
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

void cDisplay::setControlPosition(hControl handle, int16_t x, int16_t y)
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;

    if (x >= 0)
        controlsTable[handle]->posX = x;
    if (y >= 0)
        controlsTable[handle]->posY = y;
}

void cDisplay::setControlSize(hControl handle, int16_t w, int16_t h)
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;

    if (w >= 0)
        controlsTable[handle]->width = w;
    if (h >= 0)
        controlsTable[handle]->height = h;
}

void cDisplay::setControlStyle(hControl handle, uint32_t style)
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;
    // sprawdzenie porpawnosci czcionki
    if (((style >> 8) & 15) > displayFontCount)
    {
        style &= ~(15 << 8);
    }

    // controlsTable[handle]->cStyle = style;
    controlsTable[handle]->setStyle(style);
}

void cDisplay::setControlStyle2(hControl handle, uint32_t style)
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;
    // sprawdzenie porpawnosci czcionki
    if (((style >> 8) & 15) > displayFontCount)
    {
        style &= ~(15 << 8);
    }

    // controlsTable[handle]->cStyle = style;
    ((cLabel*)controlsTable[handle])->setStyle2(style);
}

void cDisplay::setControlShow(hControl handle)
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;

    controlsTable[handle]->style |= (controlStyleShow);

    // uint32_t temp_style = controlsTable[handle]->style;
    // temp_style &= ~(controlStyleShow);
    // controlsTable[handle]->setStyle(style);
}

void cDisplay::setControlHide(hControl handle)
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;

    controlsTable[handle]->style &= ~(controlStyleShow);
}

void cDisplay::setAddControlStyle(hControl handle, uint32_t style)
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;

    uint32_t temp_style = controlsTable[handle]->style;
    temp_style |= (style);
    controlsTable[handle]->setStyle(temp_style);
}

void cDisplay::setRemoveControlStyle(hControl handle, uint32_t style)
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;

    uint32_t temp_style = controlsTable[handle]->style;
    temp_style &= ~(style);
    controlsTable[handle]->setStyle(temp_style);
}

void cDisplay::setControlText(hControl handle, const char* text)
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;
    controlsTable[handle]->setText(text);
}

void cDisplay::setControlText2(hControl handle, const char* text)
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;
    ((cLabel*)controlsTable[handle])->setText2(text);
}

void cDisplay::setControlValue(hControl handle, int value)
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;
    controlsTable[handle]->setValue(value);
}

void cDisplay::setControlColors(hControl handle, const uint32_t colorsTable[])
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;
    for (uint8_t i = 0; i < controlsTable[handle]->colorsCount; i++)
        if (colorsTable + i == nullptr || colorsTable[i] > 0xFFFFFF)
            return;

    controlsTable[handle]->setColors(colorsTable);
}

void cDisplay::setControlData(hControl handle, void* data)
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;
    controlsTable[handle]->setData(data);
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cDisplay::setModulesControlDestroyPoint()
{
    int8_t found = -1;

    for (uint8_t i = 0; i < controlsCount; i++)
    {
        found = i; // punkt jako nastepna kontrolka w tablicy
        if (controlsTable[i] == nullptr)
            break;
    }

    destroyPoint = found;
}

void cDisplay::destroyModulesControls()
{
    if (destroyPoint < 0 || destroyPoint >= controlsCount)
        return;

    for (uint8_t i = destroyPoint; i < controlsCount; i++)
    {
        delete controlsTable[i];
        controlsTable[i] = nullptr;
    }
}

void cDisplay::freeControlResources(int8_t controlsTableIndex)
{
    if (controlsTableIndex < 0 || controlsTableIndex > controlsCount - 1)
        return;

    delete controlsTable[controlsTableIndex];
    controlsTable[controlsTableIndex] = nullptr;

    // przesun wszystkie wskazniki w tablicy powyzej kasowanego
    for (int8_t i = controlsTableIndex; i < controlsCount - 1; i++)
    {
        controlsTable[i] = controlsTable[i + 1];
    }

    controlsTable[controlsCount - 1] = nullptr; // ostatnia pozycja tablicy moze byc zerowana
}
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cDisplay::destroyControl(hControl handle)
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;

    freeControlResources(handle);
}

void cDisplay::refreshControl(hControl handle)
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;

    // dorzuc kontrolke do kolejki fifo odtwarzania
    // jesli juz jest w kolejce to nic nie rob
    // jesli aktualnie odswiezana - zresetuj proces odswiezania

    // przeszukaj kolejke
    uint8_t i = refreshQueueBott;
    //
    if (refreshQueue[i] == handle)
    {
        if (refreshQueueTop != i)
        {
            i++;
            if (i >= controlsRefreshQueueSize)
                i = 0;
        }
    }
    // przeszukaj reszte kolejki
    while (i != refreshQueueTop) // ryzykowne ale optymalne
    {
        if (handle == refreshQueue[i])
        {
            return; // znaleziono w kolejce - wyjdz
        }
        i++;
        if (i >= controlsRefreshQueueSize)
            i = 0;
    }

    // dodaj kontorlke do kolejki
    refreshQueue[refreshQueueTop] = handle;
    refreshQueueTop++;
    if (refreshQueueTop >= controlsRefreshQueueSize)
    {
        refreshQueueTop = 0;
    }
}

void cDisplay::synchronizeRefresh()
{
    if (refreshQueueTop == refreshQueueBott)
    {
        stopAppend = false;
        // synchQueuePosition = -1;
        return;
    }

    synchQueuePosition = refreshQueueTop;
    stopAppend = true;
}

void cDisplay::setClearRefreshFlag(hControl handle)
{
    if (handle < 0 || handle >= controlsCount || controlsTable[handle] == nullptr)
        return;

    controlsTable[handle]->refresh = 0;
}

void cDisplay::resetControlQueue()
{
    stopAppend = false;
    refreshQueueTop = 0;
    refreshQueueBott = 0;

    updateStep = 0;
    actualUpdating = nullptr;
}

uint8_t cDisplay::isIdle()
{
    return (updateStep == 0) && (refreshQueueTop == refreshQueueBott);
}

void cDisplay::forceAppedStage()
{
    updateStep = 2;
}

void cDisplay::processDisplayUpdateImmediate()
{
    if (isControlsRefreshProcessIdle())
    {
        return;
    }

    if (refreshQueueTop == refreshQueueBott)
    {
        return;
    }

    uint8_t queuePosition = refreshQueueTop;

    while (refreshQueueBott != queuePosition && refreshQueueTop != refreshQueueBott)
    {
        update();
    }
}

bool cDisplay::isControlsRefreshProcessIdle()
{
    return (updateStep == 0) && (refreshQueueTop == refreshQueueBott);
}
//=====================================================================================================
// jpg/png
//=====================================================================================================
uint8_t cDisplay::readImgFromSd(char* path, uint32_t address, int16_t max_w, int16_t max_h)
{
    FileSystem* fileSystem = TrackerImpl::getInstance()->getPlatform()->getFileSystem();

    strcat(path, ".jpg");

    if (!fileSystem->exists(path))
    {
        path[strlen(path) - 3] = 0;
        strcat(path, "jpeg");

        if (!fileSystem->exists(path))
        {
            return 1;
            // path[strlen(path)-4] = 0;
            // strcat(path,"png");
            // if(!SD.exists(path))
            //{
            //	return;
            //}
        }
    }

    if (img.status == 1 || img.status == 2)
    {
        return 1;
    }

    auto [isOpen, fileHandle] =
      fileSystem->openFile(path, FileSystem ::FileOpenMode::kRead, &imgFile);

    if (!isOpen)
    {
        return 1;
    }

    int32_t readBytes = 0;
    uint8_t currentBuffer[5000];
    readBytes = fileSystem->readFile(currentBuffer, 5000, imgFile);

    if (readBytes < 0)
    {
        fileSystem->closeFile(imgFile, false);
        return 1;
    }

    uint16_t width = 0;
    uint16_t height = 0;

    if (get_jpeg_size(currentBuffer, readBytes, &width, &height) == 0)
    {
        fileSystem->closeFile(imgFile, false);
        return 1;
    }

    if (width == 0 || width > max_w || height == 0 || height > max_h)
    {
        fileSystem->closeFile(imgFile, false);
        return 1;
    }

    img.size = fileSystem->getFileSize(imgFile);
    img.address = address;

    if (img.size > 1000000 - address)
    {
        fileSystem->closeFile(imgFile, false);
        return 1;
    }

    fileSystem->seekFile(0, imgFile);

    img.type = 0;
    img.loadProgress = 0;
    img.progressMax = img.size / imgBufforSize;
    img.status = 1;

    loadImage = 1;

    return 0;
}

void cDisplay::readImgFromMemory(uint8_t* data, uint32_t size) // todo
{
    loadImage = 1;

    img.type = 0;
    img.loadProgress = 0;
    img.data = data;
    img.size = size;
    img.progressMax = img.size / imgBufforSize;
    img.status = 1;
}

//=====================================================================================================
// grupowe
//=====================================================================================================
void cDisplay::hideAllControls()
{
    for (uint8_t i = 0; i < controlsCount; i++)
    {
        if (controlsTable[i] != nullptr)
        {
            controlsTable[i]->style &= ~(controlStyleShow);
        }
    }
}

void cDisplay::clear()
{
    displayDriver->beginCoProList();

    API_CMD_DLSTART();

    API_CLEAR_COLOR(0x000000);
    API_CLEAR(1, 1, 1);

    API_DISPLAY();
    API_CMD_SWAP();

    displayDriver->endCoProList();
}
//=====================================================================================================
// debug
//=====================================================================================================

void cDisplay::printActualControlTableContent()
{
    LOG("");
    LOG("-----------------------------------------------------------");
    LOGF("%s", "   CONTROLS TABLE");
    LOGF("%s", "       DP: ");
    LOGF("%d\n", destroyPoint);
    LOG("-----------------------------------------------------------");
    LOG("      id                 show  style     ram size    refresh");

    char idString[20];
    char logString[100];

    for (uint8_t i = 0; i < controlsCount; i++)
    {
        if (controlsTable[i] == nullptr)
        {
            LOG("");
            continue;
        }

        controlIdToString(controlsTable[i]->id, idString);

        sprintf(logString,
                " %-3u| %-17s| %-4u| %-8u| %-10u| %-5u|",
                i,
                idString,
                (unsigned int)controlsTable[i]->style & 1,
                (unsigned int)controlsTable[i]->style,
                (unsigned int)controlsTable[i]->ramSize,
                (unsigned int)controlsTable[i]->refresh);

        LOG(logString);
    }

    LOG("");
}

void cDisplay::controlIdToString(uint8_t id, char* string)
{
    switch (id)
    {
        case controlIdBar: strcpy(string, "bar"); break;
        case controlIdBgLabel: strcpy(string, "bgLabel"); break;
        case controlIdFrame: strcpy(string, "frame"); break;
        case controlIdEdit: strcpy(string, "edit"); break;
        case controlIdImg: strcpy(string, "img"); break;
        case controlIdLabel: strcpy(string, "label"); break;
        case controlIdHorizontalBar: strcpy(string, "horizontalBar"); break;
        case controlIdLineIndicator: strcpy(string, "lineIndicator"); break;
        case controlIdList: strcpy(string, "list"); break;
        case controlIdKeyboard: strcpy(string, "keyboard"); break;
        case controlIdNotePopup: strcpy(string, "notePopup"); break;
        case controlIdMultiLabel: strcpy(string, "multiLabel"); break;
        case controlIdParamValueList: strcpy(string, "paramValueList"); break;
        case controlIdPoints: strcpy(string, "waveformPoints"); break;
        case controlIdProcessingPopup: strcpy(string, "processingPopup"); break;
        case controlIdProgressCursor: strcpy(string, "progressCoursor"); break;
        case controlIdSimpleText: strcpy(string, "simpleText"); break;
        case controlIdSlicePoints: strcpy(string, "slicePoints"); break;
        case controlIdSongPlayer: strcpy(string, "player"); break;
        case controlIdSpectrum: strcpy(string, "waveform"); break;
        case controlIdStartScreen: strcpy(string, "startScreen"); break;
        case controlIdTemplate: strcpy(string, "template"); break;
        case controlIdTextBox: strcpy(string, "textBox"); break;
        case controlIdTextPopup: strcpy(string, "textPopup"); break;
        case controlIdTracker: strcpy(string, "tracker"); break;
        case controlIdWtProgress: strcpy(string, "progress"); break;
        case controlIdNotesKeyboardLabel: strcpy(string, "notesKeyboard"); break;
        default: strcpy(string, "undefined"); break;
    }
}

//=====================================================================================================
// hardware
//=====================================================================================================
void cDisplay::setBacklightBrightness(uint8_t value)
{
    //
    backlightBrightness = value;
    updateDisplaySettings = 1;
}

void cDisplay::setRotate(uint8_t value)
{
    //
    rotateValue = value;
    updateDisplaySettings = 1;
}

void cDisplay::turnOn()
{
    displayDriver->setPowerState(true);
}

void cDisplay::turnOff()
{
    displayDriver->setPowerState(false);
}

void cDisplay::setTheme(uint8_t index)
{
    switch (index)
    {
        case 0: theme = &themeDefault; break;
        case 1: theme = &themeWhiteout; break;
        case 2: theme = &themeAnniversary; break;
        default: theme = &themeDefault; break;
    }
}

void cDisplay::setCustomTheme(const Theme& customTheme)
{
    themeCustom = customTheme;
    theme = &themeCustom;
}

#pragma pack(push, 1)
struct PtFontHeader
{
    char magic[4];       // "PTFN"
    uint16_t version;    // 1
    uint16_t fontCount;  // 5
    uint8_t reserved[8];
};

struct PtFontEntry
{
    uint32_t offset;
    uint32_t size;
    uint16_t handle;
    uint16_t format;
};
#pragma pack(pop)

bool cDisplay::loadCustomFontFromSD(FileSystem* fs, const char* fontFilePath)
{
    if (!fs || !fontFilePath)
        return false;

    auto [isOpen, file] = fs->openFile(fontFilePath, FileSystem::FileOpenMode::kRead);
    if (!isOpen || !file)
    {
        const char* alt = (fontFilePath[0] == '/') ? fontFilePath + 1 : fontFilePath;
        std::tie(isOpen, file) = fs->openFile(alt, FileSystem::FileOpenMode::kRead);
    }
    if (!isOpen || !file)
        return false;

    PtFontHeader header;
    if (fs->readFile(&header, sizeof(header), file) != static_cast<int32_t>(sizeof(header)))
    {
        fs->closeFile(file);
        return false;
    }

    if (memcmp(header.magic, "PTFN", 4) != 0 || header.version != 1 || header.fontCount != displayFontCount)
    {
        fs->closeFile(file);
        return false;
    }

    PtFontEntry entries[displayFontCount];
    if (fs->readFile(entries, sizeof(entries), file) != static_cast<int32_t>(sizeof(entries)))
    {
        fs->closeFile(file);
        return false;
    }

    displayDriver->beginCoProList();
    API_CMD_DLSTART();
    API_CLEAR_COLOR(0x000000);
    API_CLEAR(1, 1, 1);
    API_DISPLAY();
    displayDriver->endCoProList();

    for (uint8_t i = 0; i < displayFontCount; i++)
    {
        if (entries[i].size > kMaxFontBufferSize)
        {
            fs->closeFile(file);
            return false;
        }

        fs->seekFile(entries[i].offset, file);
        if (fs->readFile(decompressedFontsCommonBuffer, entries[i].size, file) !=
            static_cast<int32_t>(entries[i].size))
        {
            fs->closeFile(file);
            return false;
        }

        uint16_t char_num = 0;
        for (int cha = 0; cha < 128; cha++)
        {
            if (decompressedFontsCommonBuffer[cha])
            {
                char_num++;
            }
        }

        memcpy(fonts[i].data, decompressedFontsCommonBuffer, 148);
        fonts[i].format = decompressedFontsCommonBuffer[128];
        fonts[i].linestride = decompressedFontsCommonBuffer[132];
        fonts[i].width = decompressedFontsCommonBuffer[136];
        fonts[i].height = decompressedFontsCommonBuffer[140];

        fonts[i].size = char_num * fonts[i].linestride * fonts[i].height + 148;
        fonts[i].handle = i + 1;

        fonts[i].address = (i == 0 ? 0 : fonts[i - 1].address + fonts[i - 1].size);
        int rest = fonts[i].address % 4;
        if (rest != 0)
        {
            fonts[i].address = fonts[i].address + (4 - rest);
        }

        fonts[i].source = fonts[i].address + 148;

        displayDriver->writeFontRAMG(decompressedFontsCommonBuffer, fonts[i].size, fonts[i].address);
    }

    fs->closeFile(file);

    displayDriver->beginCoProList();
    API_CMD_DLSTART();

    API_CLEAR_COLOR(0x000000);
    API_CLEAR(1, 1, 1);

    uint8_t first_char;
    for (uint8_t i = 0; i < displayFontCount; i++)
    {
        first_char = 0;

        while (!fonts[i].data[first_char] && first_char < 127)
            first_char++;

        API_BITMAP_HANDLE(fonts[i].handle);
        API_BITMAP_SOURCE(fonts[i].source);
        API_BITMAP_LAYOUT(L4, fonts[i].linestride, fonts[i].height);
        API_BITMAP_SIZE(NEAREST, BORDER, BORDER, fonts[i].width, fonts[i].height);
        API_CMD_SETFONT2(fonts[i].handle, fonts[i].address, first_char);
    }

    API_DISPLAY();
    API_CMD_SWAP();
    displayDriver->endCoProList();

    return true;
}

void cDisplay::setFont(uint8_t index)
{
    switch (index)
    {
        case 0: loadFont(fontsData1, displayFontCount); break;
        case 1: loadFont(fontsData2, displayFontCount); break;
        default: loadFont(fontsData1, displayFontCount); break;
    }
}

void cDisplay::showBatteryScreen(bool empty, bool charging, int8_t level, float voltage)
{
    displayDriver->beginCoProList();

    API_CMD_DLSTART();

    API_CLEAR_COLOR(0x000000);
    API_CLEAR(1, 1, 1);

    API_VERTEX_FORMAT(0);

    API_COLOR(0xFFFFFF);

    if (empty)
    {
        API_CMD_TEXT(
          400, 285, 27, OPT_CENTER, charging ? "Low battery, charging..." : "Low battery");
    }
    else
    {
        if (charging)
        {
            API_CMD_TEXT(400, 285, 27, OPT_CENTER, "Charging...");
        }
#if defined(DEBUG)
        char text[50];
        sprintf(text, "Battery level: %d%%, voltage: %.2fV", level, voltage);
        API_CMD_TEXT(400, 320, 29, OPT_CENTER, text);
#endif
    }

    const uint16_t batW = 75;
    const uint16_t batH = 30;
    const uint16_t batX = (800 - batW) / 2;
    const uint16_t batY = (480 - batH) / 2;

    API_COLOR(level >= 20 ? 0x00FF00 : 0xFF0000);
    API_BEGIN(RECTS);
    API_VERTEX2F(batX, batY + 1);
    API_VERTEX2F(batX + (((batW - 4) * level) / 100), batY + (batH - 2));
    API_END();

    API_COLOR(0x888888);
    API_BEGIN(LINE_STRIP);
    API_VERTEX2F(batX, batY);
    API_VERTEX2F(batX + batW, batY);
    API_VERTEX2F(batX + batW, batY + batH);
    API_VERTEX2F(batX, batY + batH);
    API_VERTEX2F(batX, batY);
    API_END();
    API_BEGIN(RECTS);
    API_VERTEX2F(batX + batW, batY + (batH / 3));
    API_VERTEX2F(batX + batW + (batH / 6), batY + batH - (batH / 3));
    API_END();

    API_DISPLAY();
    API_CMD_SWAP();

    displayDriver->endCoProList();

    displayDriver->awaitCoProEmpty();
}

uint8_t* cDisplay::decompressFont(const CompressedFontData* compressedFontData)
{
    decompress(compressedFontData->data, compressedFontData->count, decompressedFontsCommonBuffer);

    return decompressedFontsCommonBuffer;
}
