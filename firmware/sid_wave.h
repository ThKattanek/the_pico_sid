//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid_wave.h                            //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
// The template used was resid:                 //
// https://github.com/libsidplayfp/resid        //
//                                              //
//////////////////////////////////////////////////

#ifndef SID_WAVE_CLASS_H
#define SID_WAVE_CLASS_H

#include <stdint.h>

#include "./pico_sid_defs.h"

class SID_WAVE
{
public:
    SID_WAVE();
    ~SID_WAVE();

    void SetSidType(sid_type type);
    void SetSyncSource(SID_WAVE* wave_source);
    void Reset();

    void Clock(int delta_t);
    void Synchronize();

    void WriteFreqLo(reg8 value);
    void WriteFreqHi(reg8 value);
    void WritePwLo(reg8 value);
    void WritePwHi(reg8 value);
    void WriteControlReg(reg8 value);
    reg8 ReadOSC();

    reg12 OutWaveform();

    short Output();

    void SetWaveformOutput();
    void SetWaveformOutput(cycle_count delta_t);

protected:
    void ClockShiftRegister();
    void WriteShiftRegister();
    void SetNoiseOutput();
    void WaveBitfade();
    void ShiftregBitfade();

    const SID_WAVE* sync_source;
    SID_WAVE* sync_dest;

    reg24 accumulator;


    bool msb_rising;
    reg24 freq;
    reg12 pw;

    reg24 shift_register;

    cycle_count shift_register_reset;
    cycle_count shift_pipeline;

    reg24 ring_msb_mask;
    unsigned short no_noise;
    unsigned short noise_output;
    unsigned short no_noise_or_noise_output;
    unsigned short no_pulse;
    unsigned short pulse_output;

    reg8 waveform;
    reg12 tri_saw_pipeline;
    reg12 osc3;
    reg8 test;
    reg8 ring_mod;
    reg8 sync;

    reg12 waveform_output;
    cycle_count floating_output_ttl;

    sid_type sid_model;

    unsigned short* wave;
    static unsigned short model_wave[2][8][1 << 12];
    static unsigned short model_dac[2][1 << 12];

    friend class SID_VOICE;
    friend class PICO_SID;
};

#endif // SID_WAVE_CLASS_H
