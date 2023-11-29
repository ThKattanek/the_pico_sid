//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid_wave.cpp                          //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
// The template used was resid:                 //
// https://github.com/libsidplayfp/resid        //
//                                              //
//////////////////////////////////////////////////

#include "./sid_wave.h"
#include "./sid_dac.h"

// Number of cycles after which the shift register is reset
// when the test bit is set.
const cycle_count SHIFT_REGISTER_RESET_START_6581 =   35000; // 0x8000
const cycle_count SHIFT_REGISTER_RESET_BIT_6581   =    1000;
const cycle_count SHIFT_REGISTER_RESET_START_8580 = 2519864; // 0x950000
const cycle_count SHIFT_REGISTER_RESET_BIT_8580   =  315000;

// Number of cycles after which the waveform output fades to 0 when setting
// the waveform register to 0.
//
// We have two SOAS/C samplings showing that floating DAC
// keeps its state for at least 0x14000 cycles.
//
// This can't be found via sampling OSC3, it seems that
// the actual analog output must be sampled and timed.
const cycle_count FLOATING_OUTPUT_TTL_START_6581 =  182000;  // ~200ms
const cycle_count FLOATING_OUTPUT_TTL_BIT_6581   =    1500;
const cycle_count FLOATING_OUTPUT_TTL_START_8580 = 4400000; // ~5s
const cycle_count FLOATING_OUTPUT_TTL_BIT_8580   =   50000;

// Waveform lookup tables.
unsigned short SID_WAVE::model_wave[2][8][1 << 12] = {
    {
        {0},
        {0},
        {0},
#include "wave6581__ST.h"
        {0},
#include "wave6581_P_T.h"
#include "wave6581_PS_.h"
#include "wave6581_PST.h"
    },
    {
        {0},
        {0},
        {0},
#include "wave8580__ST.h"
        {0},
#include "wave8580_P_T.h"
#include "wave8580_PS_.h"
#include "wave8580_PST.h"
    }
};

// DAC lookup tables.
unsigned short SID_WAVE::model_dac[2][1 << 12] = {
    {0},
    {0},
};


SID_WAVE::SID_WAVE()
{
    static bool class_init = false;

    if (!class_init)
    {
        // Calculate tables for normal waveforms.
        accumulator = 0;
        for (int i = 0; i < (1 << 12); i++) {
            reg24 msb = accumulator & 0x800000;

            // Noise mask, triangle, sawtooth, pulse mask.
            // The triangle calculation is made branch-free, just for the hell of it.
            model_wave[0][0][i] = model_wave[1][0][i] = 0xfff;
            model_wave[0][1][i] = model_wave[1][1][i] = ((accumulator ^ -!!msb) >> 11) & 0xffe;
            model_wave[0][2][i] = model_wave[1][2][i] = accumulator >> 12;
            model_wave[0][4][i] = model_wave[1][4][i] = 0xfff;

            accumulator += 0x1000;
        }

        // Build DAC lookup tables for 12-bit DACs.
        // MOS 6581: 2R/R ~ 2.20, missing termination resistor.
        BuildDacTable(model_dac[0], 12, 2.20, false);
        // MOS 8580: 2R/R ~ 2.00, correct termination.
        BuildDacTable(model_dac[1], 12, 2.00, true);

        unsigned short max0 = model_dac[0][0];
        unsigned short min0 = model_dac[0][0];

        unsigned short max1 = model_dac[1][0];
        unsigned short min1 = model_dac[1][0];

        for(int i=1; i<(1<<12); i++)
        {
            if(max0 < model_dac[0][i])
                max0 = model_dac[0][i];
            if(min0 > model_dac[0][i])
                min0 = model_dac[0][i];

            if(max1 < model_dac[1][i])
                max1 = model_dac[1][i];
            if(min1 > model_dac[1][i])
                min1 = model_dac[1][i];
        }

        class_init = true;
    }

    sync_source = this;

    sid_model = MOS_6581;

    // Accumulator's even bits are high on powerup
    accumulator = 0x555555;

    tri_saw_pipeline = 0x555;

    Reset();
}

SID_WAVE::~SID_WAVE()
{

}

void SID_WAVE::SetSidType(sid_type type)
{
    sid_model = type;
    wave = model_wave[type][waveform & 0x7];
}

void SID_WAVE::SetSyncSource(SID_WAVE *wave_source)
{
    sync_source = wave_source;
    wave_source->sync_dest = this;
}

void SID_WAVE::Reset()
{
    // accumulator is not changed on reset
    freq = 0;
    pw = 0;

    msb_rising = false;

    waveform = 0;
    test = 0;
    ring_mod = 0;
    sync = 0;

    wave = model_wave[sid_model][0];

    ring_msb_mask = 0;
    no_noise = 0xfff;
    no_pulse = 0xfff;
    pulse_output = 0xfff;

    // reset shift register
    // when reset is released the shift register is clocked once
    shift_register = 0x7ffffe;
    shift_register_reset = 0;
    SetNoiseOutput();

    shift_pipeline = 0;

    waveform_output = 0;
    osc3 = 0;
    floating_output_ttl = 0;
}

void SID_WAVE::WriteFreqLo(reg8 value)
{
    freq = (freq & 0xff00) | (value & 0x00ff);
}

void SID_WAVE::WriteFreqHi(reg8 value)
{
    freq = ((value << 8) & 0xff00) | (freq & 0x00ff);
}

void SID_WAVE::WritePwLo(reg8 value)
{
    pw = (pw & 0xf00) | (value & 0x0ff);
    // Push next pulse level into pulse level pipeline.
    pulse_output = (accumulator >> 12) >= pw ? 0xfff : 0x000;
}

void SID_WAVE::WritePwHi(reg8 value)
{
    pw = ((value << 8) & 0xf00) | (pw & 0x0ff);
    // Push next pulse level into pulse level pipeline.
    pulse_output = (accumulator >> 12) >= pw ? 0xfff : 0x000;
}

bool DoPreWriteback(reg8 waveform_prev, reg8 waveform, bool is6581)
{
    // no writeback without combined waveforms
    if (likely(waveform_prev <= 0x8))
        return false;
    // This need more investigation
    if (waveform == 8)
        return false;
    if (waveform_prev == 0xc) {
        if (is6581)
            return false;
        else if ((waveform != 0x9) && (waveform != 0xe))
            return false;
    }
    // What's happening here?
    if (is6581 &&
        ((((waveform_prev & 0x3) == 0x1) && ((waveform & 0x3) == 0x2))
         || (((waveform_prev & 0x3) == 0x2) && ((waveform & 0x3) == 0x1))))
        return false;
    // ok do the writeback
    return true;
}

void SID_WAVE::WriteControlReg(reg8 value)
{
    reg8 waveform_prev = waveform;
    reg8 test_prev = test;
    waveform = (value >> 4) & 0x0f;
    test = value & 0x08;
    ring_mod = value & 0x04;
    sync = value & 0x02;

    // Set up waveform table.
    wave = model_wave[sid_model][waveform & 0x7];

    // Substitution of accumulator MSB when sawtooth = 0, ring_mod = 1.
    ring_msb_mask = ((~value >> 5) & (value >> 2) & 0x1) << 23;

    // no_noise and no_pulse are used in set_waveform_output() as bitmasks to
    // only let the noise or pulse influence the output when the noise or pulse
    // waveforms are selected.
    no_noise = waveform & 0x8 ? 0x000 : 0xfff;
    no_noise_or_noise_output = no_noise | noise_output;
    no_pulse = waveform & 0x4 ? 0x000 : 0xfff;

    // Test bit rising.
    // The accumulator is cleared, while the the shift register is prepared for
    // shifting by interconnecting the register bits. The internal SRAM cells
    // start to slowly rise up towards one. The SRAM cells reach one within
    // approximately $8000 cycles, yielding a shift register value of
    // 0x7fffff.
    if (!test_prev && test) {
        // Reset accumulator.
        accumulator = 0;

        // Flush shift pipeline.
        shift_pipeline = 0;

        // Set reset time for shift register.
        shift_register_reset = (sid_model == MOS_6581) ? SHIFT_REGISTER_RESET_START_6581 : SHIFT_REGISTER_RESET_START_8580;

        // The test bit sets pulse high.
        pulse_output = 0xfff;
    }
    else if (test_prev && !test) {
        // When the test bit is falling, the second phase of the shift is
        // completed by enabling SRAM write.

        // During first phase of the shift the bits are interconnected
        // and the output of each bit is latched into the following.
        // The output may overwrite the latched value.
        if (DoPreWriteback(waveform_prev, waveform, sid_model == MOS_6581)) {
            WriteShiftRegister();
        }

        // bit0 = (bit22 | test) ^ bit17 = 1 ^ bit17 = ~bit17
        reg24 bit0 = (~shift_register >> 17) & 0x1;
        shift_register = ((shift_register << 1) | bit0) & 0x7fffff;

        // Set new noise waveform output.
        SetNoiseOutput();
    }

    if (waveform) {
        // Set new waveform output.
        SetWaveformOutput();
    }
    else if (waveform_prev) {
        // Change to floating DAC input.
        // Reset fading time for floating DAC input.
        floating_output_ttl = (sid_model == MOS_6581) ? FLOATING_OUTPUT_TTL_START_6581 : FLOATING_OUTPUT_TTL_START_8580;
    }

    // The gate bit is handled by the EnvelopeGenerator.
}

void SID_WAVE::WaveBitfade()
{
    waveform_output &= waveform_output >> 1;
    osc3 = waveform_output;
    if (waveform_output != 0)
        floating_output_ttl = (sid_model == MOS_6581) ? FLOATING_OUTPUT_TTL_BIT_6581 : FLOATING_OUTPUT_TTL_BIT_8580;
}

void SID_WAVE::ShiftregBitfade()
{
    shift_register |= 1;
    shift_register |= shift_register << 1;

    // New noise waveform output.
    SetNoiseOutput();
    if (shift_register != 0x7fffff)
        shift_register_reset = (sid_model == MOS_6581) ? SHIFT_REGISTER_RESET_BIT_6581 : SHIFT_REGISTER_RESET_BIT_8580;
}

reg8 SID_WAVE::ReadOSC()
{
    return osc3 >> 4;
}

reg12 SID_WAVE::OutWaveform()
{
    return waveform_output;
}
