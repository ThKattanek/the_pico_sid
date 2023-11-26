//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid_filter.h                          //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
// The template used was resid:                 //
// https://github.com/libsidplayfp/resid        //
//                                              //
//////////////////////////////////////////////////

#ifndef SID_FILTER_CLASS_H
#define SID_FILTER_CLASS_H

#include <stdint.h>

#include "./pico_sid_defs.h"

class SID_FILTER
{
public:
    SID_FILTER();
    ~SID_FILTER();

    void SetSidType(sid_type type);
    void Reset();

    void WriteFcLo(reg8 value);
    void WriteFcHi(reg8 value);
    void WriteResFilt(reg8 value);
    void WriteModeVol(reg8 value);

    void Clock(cycle_count delta_t, int voice1, int voice2, int voice3);
    short Output();
};

inline void SID_FILTER::Clock(cycle_count delta_t, int voice1, int voice2, int voice3)
{

}

inline short SID_FILTER::Output()
{
    return 0;
}

#endif // SID_FILTER_CLASS_H
