//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid_envelope.h                        //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
// The template used was resid:                 //
// https://github.com/libsidplayfp/resid        //
//                                              //
//////////////////////////////////////////////////

#ifndef SID_ENVELOPE_CLASS_H
#define SID_ENVELOPE_CLASS_H

#include <stdint.h>

#include "./pico_sid_defs.h"

class SID_ENVELOPE
{
public:
    SID_ENVELOPE();
    ~SID_ENVELOPE();

    void SetSidType(sid_type type);

    void Reset();


};

#endif // SID_ENVELOPE_CLASS_H
