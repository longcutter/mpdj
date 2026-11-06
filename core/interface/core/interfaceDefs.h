
#ifndef MT_INTERFACE_DEFS_H
#define MT_INTERFACE_DEFS_H

#include "config.h"
#include "displayControls.h"
#include "mtSequencer.h"
#include "mtStructs.h"

#include "interfaceFxDesc.h"

#if defined(PLATFORM_IMX) && defined(FEATURE_SYNTH)
#define GUI_HANDLE_SYNTH 1
#else
#define GUI_HANDLE_SYNTH 0
#endif

//#define MT_INTERFACE_STARTUP_TIME 10000

//-------------------------------------------------------------
enum mtInterfacePadsDef
{
    // 1
    interfacePad0,
    interfacePad1,
    interfacePad2,
    interfacePad3,
    interfacePad4,
    interfacePad5,
    interfacePad6,
    interfacePad7,
    // 2
    interfacePad8,
    interfacePad9,
    interfacePad10,
    interfacePad11,
    interfacePad12,
    interfacePad13,
    interfacePad14,
    interfacePad15,
    // 3
    interfacePad16,
    interfacePad17,
    interfacePad18,
    interfacePad19,
    interfacePad20,
    interfacePad21,
    interfacePad22,
    interfacePad23,
    // 4
    interfacePad24,
    interfacePad25,
    interfacePad26,
    interfacePad27,
    interfacePad28,
    interfacePad29,
    interfacePad30,
    interfacePad31,
    // 5
    interfacePadKey0,

    interfacePadsCount = 48,
};

enum mtInterfaceButtonsDef
{
    interfaceButton0,
    interfaceButton1,
    interfaceButton2,
    interfaceButton3,
    interfaceButton4,
    interfaceButton5,
    interfaceButton6,
    interfaceButton7,

    interfaceButton8,
    interfaceButton9,
    interfaceButton10,
    interfaceButton11,
    interfaceButton12,

    interfaceButton13,
    interfaceButton14,
    interfaceButton15,
    interfaceButton16,
    interfaceButton17,

    interfaceButton18,
    interfaceButton19,
    interfaceButton20,
    interfaceButton21,
    interfaceButton22,

    interfaceButton23,
    interfaceButton24,
    interfaceButton25,
    interfaceButton26,
    interfaceButton27,

    interfaceButton28,
    interfaceButton29,
    interfaceButton30,
    interfaceButton31,
    interfaceButton32,

    // TRACKER MINI
    interfaceButton33, // srokdowy
    interfaceButton34, // menu
    interfaceButton35, // fn1
    interfaceButton36, // fn2
    interfaceButton37, // fn3
    interfaceButton38, // fn4
    interfaceButton39, // +10
    interfaceButton40, // +1
    interfaceButton41, // -1
    interfaceButton42, // -10

    interfaceButton43, // game

    interfaceButtonsCount
};

enum mtInterfacePotsDef
{
    interfacePot0,

    interfacePotsCount
};

enum mtInterfaceButtonsFunctions
{
    //  stary układ
    //	interfaceButtonPlay 		= interfaceButton23,
    //	interfaceButtonRec 			= interfaceButton24,
    //	interfaceButtonCopy 		= interfaceButton25,
    //	interfaceButtonUp			= interfaceButton26,
    //	interfaceButtonPaste 		= interfaceButton27,
    //
    //	interfaceButtonShift 		= interfaceButton28,
    //	interfaceButtonEnter 		= interfaceButton29,
    //	interfaceButtonLeft 		= interfaceButton30,
    //	interfaceButtonDown 		= interfaceButton31,
    //	interfaceButtonRight 		= interfaceButton32,

    interfaceButtonPlay = interfaceButton23,
    interfaceButtonCopy = interfaceButton24,
    interfaceButtonInsert = interfaceButton25,
    interfaceButtonUp = interfaceButton26,
    interfaceButtonDelete = interfaceButton27,

    interfaceButtonRec = interfaceButton28,
    interfaceButtonShift = interfaceButton29,
    interfaceButtonLeft = interfaceButton30,
    interfaceButtonDown = interfaceButton31,
    interfaceButtonRight = interfaceButton32,

    interfaceButtonParams = interfaceButton8,
    interfaceButtonPerformance = interfaceButton9,
    interfaceButtonFile = interfaceButton10,
    interfaceButtonConfig = interfaceButton11,
    interfaceButtonMaster = interfaceButton12,
    interfaceButtonSampleLoad = interfaceButton13,
    interfaceButtonSamplePlay = interfaceButton14,
    interfaceButtonSampleEdit = interfaceButton15,
    interfaceButtonSampleRec = interfaceButton16,
    interfaceButtonSong = interfaceButton17,
    interfaceButtonNote = interfaceButton18,
    interfaceButtonInstr = interfaceButton19,
    interfaceButtonVol = interfaceButton20, // <<
    interfaceButtonFx = interfaceButton21,  // <<
    interfaceButtonPattern = interfaceButton22,

    interfaceButtonGame = interfaceButton43,

    interfaceButtonFx1 = interfaceButton21,
    interfaceButtonFx2 = interfaceButton20,

    interfaceButtonMiniMiddle = interfaceButton33,  // srokdowy
    interfaceButtonMiniMenu = interfaceButton34,    // menu
    interfaceButtonMiniFn1 = interfaceButton35,     // fn1
    interfaceButtonMiniFn2 = interfaceButton36,     // fn2
    interfaceButtonMiniFn3 = interfaceButton37,     // fn3
    interfaceButtonMiniFn4 = interfaceButton38,     // fn4
    interfaceButtonMiniPlus10 = interfaceButton39,  // +10
    interfaceButtonMiniPlus1 = interfaceButton40,   // +1
    interfaceButtonMiniMinus1 = interfaceButton41,  // -1
    interfaceButtonMiniMinus10 = interfaceButton42, // -10

};

//-------------------------------------------------------------
enum buttonStates
{
    buttonRelease = 0,
    buttonPress,
    buttonHold,
    buttonDouble,
};

enum padStates
{
    padRelease,
    padPress,
    padChange,
};

//-------------------------------------------------------------
enum interfacePopups
{
    mtNotePopup,
    mtIntrPopup,
    mtVolPopup,
    mtFxPopup,
    mtGlobalPopup,

};

enum DisplayThemes
{
    kOriginal = 0,
    kMonochrome,
    kAnniversary,

    kCount
};

const char fileManagerPopupText[][30] = {
    "",
    "Loading files list",
    "Loading projects list",
    "Loading firmwares list",
    "Importing sample to Project",
    "Copying sample",
    "Deleting Instruments",
    "Sample playback",
    "Loading project",
    "Loading project",
    "Saving project",
    "Saving project",
    "Loading pattern",
    "Exporting Song",
    "Exporting Song Stems",
    "Exporting Pattern",
    "Exporting Pattern Stems",
    "Exporting Selection",
    "Saving Recording",
    "Importing Recording",
    "Saving Sample",

    "Importing mod file",
    "Exporting to .it file",

    "Loading Samples",

    "Exporting Samples",
};
//-------------------------------------------------------------

enum interfaceEvents
{
    eventNone,
    eventLoadLastProjFailed,
    eventEventPadPress,
    eventLoadSampleBank,

    eventSwitchModule,
    eventSwitchToPreviousModule,
    eventSwitchModuleWithOptions,
    eventToggleActiveModule,

    eventActivateImageViewer,
    eventActivateGameModule,

    eventSetFileTransferMode,

    eventActivateTestingProcedure,

    interfaceEventsCount
};

enum enumMtOperatingMode
{
    mtOperatingModeNone,
    mtOperatingModeStartup,
    mtOperatingModeOpenProject,
    mtOperatingModeShowStartScreen,
    mtOperatingModeRun,

    mtOperatingModePowerOffSequence,
    mtOperatingModeSleep,

};

struct strSpectrumParams
{
    int16_t* address = nullptr;
    uint32_t length = 0;
    uint8_t recordInProgressFlag = 0;
};

struct strZoomParams
{
    uint16_t zoomPosition = 0;
    uint16_t zoomWidth = MAX_16BIT;
    int32_t zoomStart = 0;
    int32_t zoomEnd = MAX_16BIT;
    float zoomValue = 1;
    int32_t zoomResolution;
    uint8_t lastChangedPoint = 0;
};

class InterfaceGlobals
{
  public:
    char* padNamesPointer[48];

    char intrumentsNames[kTotalInstrumentsCount][SAMPLE_NAME_SIZE + 6];
    char* ptrIntrumentsNames[kTotalInstrumentsCount];

    //-----------------------------------------------------
    char currentFirmwareVersion[32];
    const char* currentFirmwareBuild = nullptr;

    //-----------------------------------------------------
#ifndef TRACKER_MINI
    static constexpr bool invertValForList = false;
#else
    static constexpr bool invertValForList = true;
#endif

    uint8_t fxIdToName(uint8_t id)
    {
        //		uint8_t count;
        for (uint8_t count = 0; count < FX_COUNT; count++)
        {
            if (fxIDs[count] == id)
                return count;
        }

        return 0;
    }
    int16_t fxNameToId(int16_t name)
    {
        if (name >= FX_COUNT)
        {
            return 0;
        }
        else if (name >= 0)
            return fxIDs[name];
        else
            return name;
    }
    uint8_t fxNameCount()
    {
        for (uint8_t count = 1; count < FX_COUNT; count++)
        {
            if (fxIDs[count] == 0)
                return count;
        }

        return 0;
    }

    static constexpr uint8_t fxIDs[FX_COUNT] = {
        Sequencer::strFxConsts::enFxType::FX_TYPE_NONE,
        Sequencer::strFxConsts::enFxType::FX_TYPE_OFF,
        Sequencer::strFxConsts::enFxType::FX_TYPE_VOLUME,
        Sequencer::strFxConsts::enFxType::FX_TYPE_PANNING,
        Sequencer::strFxConsts::enFxType::FX_TYPE_MICROTUNING,
        Sequencer::strFxConsts::enFxType::FX_TYPE_GLIDE,
        Sequencer::strFxConsts::enFxType::FX_TYPE_TEMPO,
        Sequencer::strFxConsts::enFxType::FX_TYPE_SWING,
        Sequencer::strFxConsts::enFxType::FX_TYPE_MICROMOVE,
        Sequencer::strFxConsts::enFxType::FX_TYPE_GATE_LENGTH,
        Sequencer::strFxConsts::enFxType::FX_TYPE_CHANCE,
        Sequencer::strFxConsts::enFxType::FX_TYPE_ROLL,
        Sequencer::strFxConsts::enFxType::FX_TYPE_ARP,
        Sequencer::strFxConsts::enFxType::FX_TYPE_RANDOM_NOTE,
        Sequencer::strFxConsts::enFxType::FX_TYPE_RANDOM_INSTRUMENT,
        Sequencer::strFxConsts::enFxType::FX_TYPE_RANDOM_VALUE,
        Sequencer::strFxConsts::enFxType::FX_TYPE_RANDOM_VOLUME,
        Sequencer::strFxConsts::enFxType::FX_TYPE_REVERSE_PLAYBACK,
        Sequencer::strFxConsts::enFxType::FX_TYPE_POSITION,
        Sequencer::strFxConsts::enFxType::FX_TYPE_SAMPLE_SLICE,
        Sequencer::strFxConsts::enFxType::FX_TYPE_VOLUME_LFO,
        Sequencer::strFxConsts::enFxType::FX_TYPE_PANNING_LFO,
        Sequencer::strFxConsts::enFxType::FX_TYPE_FILTER_LFO,
        Sequencer::strFxConsts::enFxType::FX_TYPE_POSITION_LFO,
        Sequencer::strFxConsts::enFxType::FX_TYPE_FINETUNE_LFO,
        Sequencer::strFxConsts::enFxType::FX_TYPE_OVERDRIVE,
        Sequencer::strFxConsts::enFxType::FX_TYPE_FILTER_LOWPASS,
        Sequencer::strFxConsts::enFxType::FX_TYPE_FILTER_BANDPASS,
        Sequencer::strFxConsts::enFxType::FX_TYPE_FILTER_HIGHPASS,
        Sequencer::strFxConsts::enFxType::FX_TYPE_DELAY_SEND,
        Sequencer::strFxConsts::enFxType::FX_TYPE_REVERB_SEND,
        Sequencer::strFxConsts::enFxType::FX_TYPE_BIT_DEPTH,
        Sequencer::strFxConsts::enFxType::FX_TYPE_TUNE,
        Sequencer::strFxConsts::enFxType::FX_TYPE_SLIDE_UP,
        Sequencer::strFxConsts::enFxType::FX_TYPE_SLIDE_DOWN,
        Sequencer::strFxConsts::enFxType::FX_TYPE_AMP_ENV,
        Sequencer::strFxConsts::enFxType::FX_TYPE_FILTER_ENV,
        Sequencer::strFxConsts::enFxType::FX_TYPE_GRANULAR_LENGTH,
        Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_A,
        Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_B,
        Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_C,
        Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_D,
        Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_E,
        Sequencer::strFxConsts::enFxType::FX_TYPE_SEND_CC_F,
        Sequencer::strFxConsts::enFxType::FX_TYPE_MIDI_CHORD,
        Sequencer::strFxConsts::enFxType::FX_TYPE_BREAK_PATTERN,

    };

    static constexpr char rndFxName[FX_NAME_SIZE + 4] = "Random Fx";

    static constexpr char fxNames[FX_COUNT][FX_NAME_SIZE + 4] = { "- - None",
                                                                  "! - Off",
                                                                  "V - Volume/Velocity",
                                                                  "P - Panning",
                                                                  "M - Micro-tune/Pitchbend",
                                                                  "G - Glide",
                                                                  "T - Tempo",
                                                                  "I - Swing",
                                                                  "m - Micro-move",
                                                                  "q - Gate Length",
                                                                  "C - Chance",
                                                                  "R - Roll",
                                                                  "A - Arp",
                                                                  "n - Random Note",
                                                                  "i - Random Instrument",
                                                                  "x - Random FX value",
                                                                  "v - Random Volume",
                                                                  "r - Reverse Sample Playback",
                                                                  "p - Position",
                                                                  "S - Slice",
                                                                  "g - Volume LFO",
                                                                  "h - Panning LFO",
                                                                  "j - Filter LFO",
                                                                  "k - Position LFO",
                                                                  "l - Finetune LFO",
                                                                  "D - Overdrive",
                                                                  "L - Low-pass",
                                                                  "B - Band-pass",
                                                                  "H - High-pass",
                                                                  "s - Delay Send",
                                                                  "t - Reverb Send",
                                                                  "E - Bit Depth",
                                                                  "U - Tune",
                                                                  "F - Slide up",
                                                                  "J - Slide down",
                                                                  "w - WT Volume Env",
                                                                  "y - WT Filter Env",
                                                                  "z - Granular Length",
#ifdef FEATURE_SYNTH
                                                                  "a - CC A / MACRO 1",
                                                                  "b - CC B / MACRO 2",
                                                                  "c - CC C / MACRO 3",
                                                                  "d - CC D / MACRO 4",
                                                                  "e - CC E / MACRO 5",
                                                                  "f - CC F / MACRO 6",
#else
                                                                  "a - CC A",
                                                                  "b - CC B",
                                                                  "c - CC C",
                                                                  "d - CC D",
                                                                  "e - CC E",
                                                                  "f - CC F",
#endif
                                                                  "0 - MIDI Chord",
                                                                  "x - Break pattern" };

    static constexpr char fxShortNames[FX_COUNT][FX_NAME_SIZE + 4] = { "None",
                                                                       "Off",
                                                                       "Volume",
                                                                       "Panning",
                                                                       "Micro-tune",
                                                                       "Glide",
                                                                       "Tempo",
                                                                       "Swing",
                                                                       "Micro-move",
                                                                       "Gate Length",
                                                                       "Chance",
                                                                       "Roll",
                                                                       "Arp",
                                                                       "Rnd Note",
                                                                       "Rnd Instrument",
                                                                       "Rnd FX value",
                                                                       "Rnd Volume",
                                                                       "Rev Playback",
                                                                       "Position",
                                                                       "Slice",
                                                                       "Vol LFO",
                                                                       "Pan LFO",
                                                                       "Filt LFO",
                                                                       "Pos LFO",
                                                                       "F.tune LFO",
                                                                       "Overdrive",
                                                                       "Low-pass",
                                                                       "Band-pass",
                                                                       "High-pass",
                                                                       "Delay Send",
                                                                       "Reverb Send",
                                                                       "Bit Depth",
                                                                       "Tune",
                                                                       "Slide up",
                                                                       "Slide down",
                                                                       "WT Vol Env",
                                                                       "WT Filt Env",
                                                                       "Gran Length",
#ifdef FEATURE_SYNTH
                                                                       "CC A / MACRO 1",
                                                                       "CC B / MACRO 2",
                                                                       "CC C / MACRO 3",
                                                                       "CC D / MACRO 4",
                                                                       "CC E / MACRO 5",
                                                                       "CC F / MACRO 6",
#else
                                                                       "CC A",
                                                                       "CC B",
                                                                       "CC C",
                                                                       "CC D",
                                                                       "CC E",
                                                                       "CC F",
#endif
                                                                       "MIDI Chord",
                                                                       "Break pattern" };

    static constexpr const char* ptrAllFxNames[FX_COUNT + 1] = {
        rndFxName,       &fxNames[0][0],  &fxNames[1][0],  &fxNames[2][0],  &fxNames[3][0],
        &fxNames[4][0],  &fxNames[5][0],  &fxNames[6][0],  &fxNames[7][0],  &fxNames[8][0],
        &fxNames[9][0],  &fxNames[10][0], &fxNames[11][0], &fxNames[12][0], &fxNames[13][0],
        &fxNames[14][0], &fxNames[15][0], &fxNames[16][0], &fxNames[17][0], &fxNames[18][0],
        &fxNames[19][0], &fxNames[20][0], &fxNames[21][0], &fxNames[22][0], &fxNames[23][0],
        &fxNames[24][0], &fxNames[25][0], &fxNames[26][0], &fxNames[27][0], &fxNames[28][0],
        &fxNames[29][0], &fxNames[30][0], &fxNames[31][0], &fxNames[32][0], &fxNames[33][0],
        &fxNames[34][0], &fxNames[35][0], &fxNames[36][0], &fxNames[37][0], &fxNames[38][0],
        &fxNames[39][0], &fxNames[40][0], &fxNames[41][0], &fxNames[42][0],
        &fxNames[43][0], &fxNames[44][0], &fxNames[45][0]
    };

    static constexpr const char* ptrAllFxNamesShort[FX_COUNT + 1] = {
        rndFxName,
        &fxShortNames[0][0],
        &fxShortNames[1][0],
        &fxShortNames[2][0],
        &fxShortNames[3][0],
        &fxShortNames[4][0],
        &fxShortNames[5][0],
        &fxShortNames[6][0],
        &fxShortNames[7][0],
        &fxShortNames[8][0],
        &fxShortNames[9][0],
        &fxShortNames[10][0],
        &fxShortNames[11][0],
        &fxShortNames[12][0],
        &fxShortNames[13][0],
        &fxShortNames[14][0],
        &fxShortNames[15][0],
        &fxShortNames[16][0],
        &fxShortNames[17][0],
        &fxShortNames[18][0],
        &fxShortNames[19][0],
        &fxShortNames[20][0],
        &fxShortNames[21][0],
        &fxShortNames[22][0],
        &fxShortNames[23][0],
        &fxShortNames[24][0],
        &fxShortNames[25][0],
        &fxShortNames[26][0],
        &fxShortNames[27][0],
        &fxShortNames[28][0],
        &fxShortNames[29][0],
        &fxShortNames[30][0],
        &fxShortNames[31][0],
        &fxShortNames[32][0],
        &fxShortNames[33][0],
        &fxShortNames[34][0],
        &fxShortNames[35][0],
        &fxShortNames[36][0],
        &fxShortNames[37][0],
        &fxShortNames[38][0],
        &fxShortNames[39][0],
        &fxShortNames[40][0],
        &fxShortNames[41][0],
        &fxShortNames[42][0],
        &fxShortNames[43][0],
        &fxShortNames[44][0],
        &fxShortNames[45][0]
    };

    static constexpr const char* const* ptrFxNames = &ptrAllFxNames[1];

    char* fxDescPtr(int16_t id)
    {
        if (id >= 0)
            return (char*)fxDescriptions[id];
        else
            return nullptr;
    }

    // oswiezanie listy plikow w sampleloaderze
    bool refreshFileExplorer = false;
    bool devMode = false;
};

typedef void (*editFunct_t)(int16_t);
typedef uint8_t (*editFunct1_t)(int16_t);

typedef struct node
{
    uint8_t isActive;
    editFunct_t editFunct;
    uint8_t reverseInput;

} select_node_t;

typedef struct node1
{
    uint8_t isActive;
    editFunct1_t editFunct;
    uint8_t reverseInput;

} select_node1_t;

extern InterfaceGlobals interfaceGlobals;

#endif
