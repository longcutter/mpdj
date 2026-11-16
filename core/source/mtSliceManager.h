#ifndef SOURCE_MTSLICEMANAGER_H_
#define SOURCE_MTSLICEMANAGER_H_
#include "mtStructs.h"

class mtSliceManager
{
  public:
    void addSlice(strInstrument* instr);
    void removeSlice(strInstrument* instr);
    void adjustSlice(strInstrument* instr, int32_t val);
    void autoSlice(strInstrument* instr);
    void equalSlice(strInstrument* instr, uint8_t n);

    uint8_t getAutoSliceProgress();
    uint8_t getAutoSliceState();

    // MPDJ (2026-11-16): Auto finds slices only between Start and End; the detector
    // works on that range, this maps its slices back onto the whole sample when done
    void mpdjAutoSliceDone();

  private:
    strInstrument* mpdjRangeInstr = nullptr;
    uint16_t mpdjRangeFrom = 0, mpdjRangeTo = 0;
};

extern mtSliceManager sliceManager;

#endif /* SOURCE_MTSLICEMANAGER_H_ */
