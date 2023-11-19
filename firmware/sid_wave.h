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

    void Clock();
    void Clock(int delta_t);
    void Synchronize();

    void WriteFreqLo(reg8 value);
    void WriteFreqHi(reg8 value);
    void WritePwLo(reg8 value);
    void WritePwHi(reg8 value);
    void WriteControlReg(reg8 value);
    reg8 ReadOSC();

    // 12-bit waveform output.
    short Output();

    // Calculate and set waveform output value.
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

    // Tell whether the accumulator MSB was set high on this cycle.
    bool msb_rising;

    // Fout  = (Fn*Fclk/16777216)Hz
    // reg16 freq;
    reg24 freq;
    // PWout = (PWn/40.95)%
    reg12 pw;

    reg24 shift_register;

    // Remaining time to fully reset shift register.
    cycle_count shift_register_reset;
    // Emulation of pipeline causing bit 19 to clock the shift register.
    cycle_count shift_pipeline;

    // Helper variables for waveform table lookup.
    reg24 ring_msb_mask;
    unsigned short no_noise;
    unsigned short noise_output;
    unsigned short no_noise_or_noise_output;
    unsigned short no_pulse;
    unsigned short pulse_output;

    // The control register right-shifted 4 bits; used for waveform table lookup.
    reg8 waveform;

    // 8580 tri/saw pipeline
    reg12 tri_saw_pipeline;
    reg12 osc3;

    // The remaining control register bits.
    reg8 test;
    reg8 ring_mod;
    reg8 sync;
    // The gate bit is handled by the EnvelopeGenerator.

    // DAC input.
    reg12 waveform_output;
    // Fading time for floating DAC input (waveform 0).
    cycle_count floating_output_ttl;

    sid_type sid_model;

    // Sample data for waveforms, not including noise.
    unsigned short* wave;
    static unsigned short model_wave[2][8][1 << 12];
    // DAC lookup tables.
    static unsigned short model_dac[2][1 << 12];

    friend class SID_VOICE;
    friend class PICO_SID;
};

#endif // SID_WAVE_CLASS_H
