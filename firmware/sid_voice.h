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

inline int SID_VOICE::Output()
{
    return (wave.Output() - wave_zero) * envelope.Output();
}

#endif // SID_VOICE_CLASS_H
