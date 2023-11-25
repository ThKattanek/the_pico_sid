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

// ----------------------------------------------------------------------------
// Amplitude modulated waveform output (20 bits).
// Ideal range [-2048*255, 2047*255].
// ----------------------------------------------------------------------------

// The output for a voice is produced by a multiplying DAC, where the
// waveform output modulates the envelope output.
//
// As noted by Bob Yannes: "The 8-bit output of the Envelope Generator was then
// sent to the Multiplying D/A converter to modulate the amplitude of the
// selected Oscillator Waveform (to be technically accurate, actually the
// waveform was modulating the output of the Envelope Generator, but the result
// is the same)".
//
//          7   6   5   4   3   2   1   0   VGND
//          |   |   |   |   |   |   |   |     |   Missing
//         2R  2R  2R  2R  2R  2R  2R  2R    2R   termination
//          |   |   |   |   |   |   |   |     |
//          --R---R---R---R---R---R---R--   ---
//          |          _____
//        __|__     __|__   |
//        -----     =====   |
//        |   |     |   |   |
// 12V ---     -----     ------- GND
//               |
//              vout
//
// Bit on:  wout (see figure in wave.h)
// Bit off: 5V (VGND)
//
// As is the case with all MOS 6581 DACs, the termination to (virtual) ground
// at bit 0 is missing. The MOS 8580 has correct termination.

RESID_INLINE
int SID_VOICE::Output()
{
    return (wave.Output() - wave_zero) * envelope.Output();
}
