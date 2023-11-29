//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid_voice.cpp                         //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
// The template used was resid:                 //
// https://github.com/libsidplayfp/resid        //
//                                              //
//////////////////////////////////////////////////

#include "./sid_voice.h"

SID_VOICE::SID_VOICE()
{
    SetSidType(MOS_6581);
}

SID_VOICE::~SID_VOICE()
{
}

void SID_VOICE::SetSidType(sid_type type)
{
    wave.SetSidType(type);
    envelope.SetSidType(type);

    switch (type) {
    case MOS_6581:
        wave_zero = 0x380;
        break;
    case MOS_8580:
        wave_zero = 0x9e0;
        break;
    default:
        break;
    }
}

void SID_VOICE::SetSyncSource(SID_VOICE *voice_source)
{
    wave.SetSyncSource(&voice_source->wave);
}

void SID_VOICE::Reset()
{
    wave.Reset();
    envelope.Reset();
}

void SID_VOICE::WriteControlReg(reg8 value)
{
    wave.WriteControlReg(value);
    envelope.WriteControlReg(value);
}
