//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid_voice.h                           //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
// The template used was resid:                 //
// https://github.com/libsidplayfp/resid        //
//                                              //
//////////////////////////////////////////////////

#ifndef SID_VOICE_CLASS_H
#define SID_VOICE_CLASS_H

#include <stdint.h>

#include "./pico_sid_defs.h"
#include "./sid_wave.h"
#include "./sid_envelope.h"

class SID_VOICE
{
public:
    SID_VOICE();
    ~SID_VOICE();

    void SetSidType(sid_type type);
    void SetSyncSource(SID_VOICE* voice_source);
    void Reset();
    void WriteControlReg(reg8 value);
    int Output();

    SID_WAVE wave;
    SID_ENVELOPE envelope;

protected:
    short wave_zero;

friend class PICO_SID;
};

#endif // SID_VOICE_CLASS_H
