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

// ----------------------------------------------------------------------------
// SID clocking - 1 cycle.
// ----------------------------------------------------------------------------
void SID_WAVE::Clock()
{
    if (unlikely(test)) {
        // Count down time to fully reset shift register.
        if (unlikely(shift_register_reset) && unlikely(!--shift_register_reset)) {
            ShiftregBitfade();
        }

        // The test bit sets pulse high.
        pulse_output = 0xfff;
    }
    else {
        // Calculate new accumulator value;
        reg24 accumulator_next = (accumulator + freq) & 0xffffff;
        reg24 accumulator_bits_set = ~accumulator & accumulator_next;
        accumulator = accumulator_next;

        // Check whether the MSB is set high. This is used for synchronization.
        msb_rising = (accumulator_bits_set & 0x800000) ? true : false;

        // Shift noise register once for each time accumulator bit 19 is set high.
        // The shift is delayed 2 cycles.
        if (unlikely(accumulator_bits_set & 0x080000)) {
            // Pipeline: Detect rising bit, shift phase 1, shift phase 2.
            shift_pipeline = 2;
        }
        else if (unlikely(shift_pipeline) && !--shift_pipeline) {
            ClockShiftRegister();
        }
    }
}

// ----------------------------------------------------------------------------
// SID clocking - delta_t cycles.
// ----------------------------------------------------------------------------
void SID_WAVE::Clock(cycle_count delta_t)
{
    if (unlikely(test)) {
        // Count down time to fully reset shift register.
        if (shift_register_reset) {
            shift_register_reset -= delta_t;
            if (unlikely(shift_register_reset <= 0)) {
                shift_register = 0x7fffff;
                shift_register_reset = 0;

                // New noise waveform output.
                SetNoiseOutput();
            }
        }

        // The test bit sets pulse high.
        pulse_output = 0xfff;
    }
    else {
        // Calculate new accumulator value;
        reg24 delta_accumulator = delta_t*freq;
        reg24 accumulator_next = (accumulator + delta_accumulator) & 0xffffff;
        reg24 accumulator_bits_set = ~accumulator & accumulator_next;
        accumulator = accumulator_next;

        // Check whether the MSB is set high. This is used for synchronization.
        msb_rising = (accumulator_bits_set & 0x800000) ? true : false;

        // NB! Any pipelined shift register clocking from single cycle clocking
        // will be lost. It is not worth the trouble to flush the pipeline here.

        // Shift noise register once for each time accumulator bit 19 is set high.
        // Bit 19 is set high each time 2^20 (0x100000) is added to the accumulator.
        reg24 shift_period = 0x100000;

        while (delta_accumulator) {
            if (likely(delta_accumulator < shift_period)) {
                shift_period = delta_accumulator;
                // Determine whether bit 19 is set on the last period.
                // NB! Requires two's complement integer.
                if (likely(shift_period <= 0x080000)) {
                    // Check for flip from 0 to 1.
                    if (((accumulator - shift_period) & 0x080000) || !(accumulator & 0x080000))
                    {
                        break;
                    }
                }
                else {
                    // Check for flip from 0 (to 1 or via 1 to 0) or from 1 via 0 to 1.
                    if (((accumulator - shift_period) & 0x080000) && !(accumulator & 0x080000))
                    {
                        break;
                    }
                }
            }

            // Shift the noise/random register.
            // NB! The two-cycle pipeline delay is only modeled for 1 cycle clocking.
           ClockShiftRegister();

            delta_accumulator -= shift_period;
        }

        // Calculate pulse high/low.
        // NB! The one-cycle pipeline delay is only modeled for 1 cycle clocking.
        pulse_output = (accumulator >> 12) >= pw ? 0xfff : 0x000;
    }
}


// ----------------------------------------------------------------------------
// Synchronize oscillators.
// This must be done after all the oscillators have been clock()'ed since the
// oscillators operate in parallel.
// Note that the oscillators must be clocked exactly on the cycle when the
// MSB is set high for hard sync to operate correctly. See SID::clock().
// ----------------------------------------------------------------------------
inline void SID_WAVE::Synchronize()
{
    // A special case occurs when a sync source is synced itself on the same
    // cycle as when its MSB is set high. In this case the destination will
    // not be synced. This has been verified by sampling OSC3.
    if (unlikely(msb_rising) && sync_dest->sync && !(sync && sync_source->msb_rising)) {
        sync_dest->accumulator = 0;
    }
}

// ----------------------------------------------------------------------------
// Waveform output.
// The output from SID 8580 is delayed one cycle compared to SID 6581;
// this is only modeled for single cycle clocking (see sid.cc).
// ----------------------------------------------------------------------------

// No waveform:
// When no waveform is selected, the DAC input is floating.
//

// Triangle:
// The upper 12 bits of the accumulator are used.
// The MSB is used to create the falling edge of the triangle by inverting
// the lower 11 bits. The MSB is thrown away and the lower 11 bits are
// left-shifted (half the resolution, full amplitude).
// Ring modulation substitutes the MSB with MSB EOR NOT sync_source MSB.
//

// Sawtooth:
// The output is identical to the upper 12 bits of the accumulator.
//

// Pulse:
// The upper 12 bits of the accumulator are used.
// These bits are compared to the pulse width register by a 12 bit digital
// comparator; output is either all one or all zero bits.
// The pulse setting is delayed one cycle after the compare; this is only
// modeled for single cycle clocking.
//
// The test bit, when set to one, holds the pulse waveform output at 0xfff
// regardless of the pulse width setting.
//

// Noise:
// The noise output is taken from intermediate bits of a 23-bit shift register
// which is clocked by bit 19 of the accumulator.
// The shift is delayed 2 cycles after bit 19 is set high; this is only
// modeled for single cycle clocking.
//
// Operation: Calculate EOR result, shift register, set bit 0 = result.
//
//                reset    -------------------------------------------
//                  |     |                                           |
//           test--OR-->EOR<--                                        |
//                  |         |                                       |
//                  2 2 2 1 1 1 1 1 1 1 1 1 1                         |
// Register bits:   2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 <---
//                      |   |       |     |   |       |     |   |
// Waveform bits:       1   1       9     8   7       6     5   4
//                      1   0
//
// The low 4 waveform bits are zero (grounded).
//

inline void SID_WAVE::ClockShiftRegister()
{
    // bit0 = (bit22 | test) ^ bit17
    reg24 bit0 = ((shift_register >> 22) ^ (shift_register >> 17)) & 0x1;
    shift_register = ((shift_register << 1) | bit0) & 0x7fffff;

    // New noise waveform output.
    SetNoiseOutput();
}

inline void SID_WAVE::WriteShiftRegister()
{
    // Write changes to the shift register output caused by combined waveforms
    // back into the shift register.
    // A bit once set to zero cannot be changed, hence the and'ing.
    // FIXME: Write test program to check the effect of 1 bits and whether
    // neighboring bits are affected.

    shift_register &=
        ~((1<<20)|(1<<18)|(1<<14)|(1<<11)|(1<<9)|(1<<5)|(1<<2)|(1<<0)) |
        ((waveform_output & 0x800) << 9) |  // Bit 11 -> bit 20
        ((waveform_output & 0x400) << 8) |  // Bit 10 -> bit 18
        ((waveform_output & 0x200) << 5) |  // Bit  9 -> bit 14
        ((waveform_output & 0x100) << 3) |  // Bit  8 -> bit 11
        ((waveform_output & 0x080) << 2) |  // Bit  7 -> bit  9
        ((waveform_output & 0x040) >> 1) |  // Bit  6 -> bit  5
        ((waveform_output & 0x020) >> 3) |  // Bit  5 -> bit  2
        ((waveform_output & 0x010) >> 4);   // Bit  4 -> bit  0

    noise_output &= waveform_output;
    no_noise_or_noise_output = no_noise | noise_output;
}

inline void SID_WAVE::SetNoiseOutput()
{
    noise_output =
        ((shift_register & 0x100000) >> 9) |
        ((shift_register & 0x040000) >> 8) |
        ((shift_register & 0x004000) >> 5) |
        ((shift_register & 0x000800) >> 3) |
        ((shift_register & 0x000200) >> 2) |
        ((shift_register & 0x000020) << 1) |
        ((shift_register & 0x000004) << 3) |
        ((shift_register & 0x000001) << 4);

    no_noise_or_noise_output = no_noise | noise_output;
}

// Combined waveforms:
// By combining waveforms, the bits of each waveform are effectively short
// circuited. A zero bit in one waveform will result in a zero output bit
// (thus the infamous claim that the waveforms are AND'ed).
// However, a zero bit in one waveform may also affect the neighboring bits
// in the output.
//
// Example:
//
//             1 1
// Bit #       1 0 9 8 7 6 5 4 3 2 1 0
//             -----------------------
// Sawtooth    0 0 0 1 1 1 1 1 1 0 0 0
//
// Triangle    0 0 1 1 1 1 1 1 0 0 0 0
//
// AND         0 0 0 1 1 1 1 1 0 0 0 0
//
// Output      0 0 0 0 1 1 1 0 0 0 0 0
//
//
// Re-vectorized die photographs reveal the mechanism behind this behavior.
// Each waveform selector bit acts as a switch, which directly connects
// internal outputs into the waveform DAC inputs as follows:
//
// * Noise outputs the shift register bits to DAC inputs as described above.
//   Each output is also used as input to the next bit when the shift register
//   is shifted.
// * Pulse connects a single line to all DAC inputs. The line is connected to
//   either 5V (pulse on) or 0V (pulse off) at bit 11, and ends at bit 0.
// * Triangle connects the upper 11 bits of the (MSB EOR'ed) accumulator to the
//   DAC inputs, so that DAC bit 0 = 0, DAC bit n = accumulator bit n - 1.
// * Sawtooth connects the upper 12 bits of the accumulator to the DAC inputs,
//   so that DAC bit n = accumulator bit n. Sawtooth blocks out the MSB from
//   the EOR used to generate the triangle waveform.
//
// We can thus draw the following conclusions:
//
// * The shift register may be written to by combined waveforms.
// * The pulse waveform interconnects all bits in combined waveforms via the
//   pulse line.
// * The combination of triangle and sawtooth interconnects neighboring bits
//   of the sawtooth waveform.
//
// This behavior would be quite difficult to model exactly, since the short
// circuits are not binary, but are subject to analog effects. Tests show that
// minor (1 bit) differences can actually occur in the output from otherwise
// identical samples from OSC3 when waveforms are combined. To further
// complicate the situation the output changes slightly with time (more
// neighboring bits are successively set) when the 12-bit waveform
// registers are kept unchanged.
//
// The output is instead approximated by using the upper bits of the
// accumulator as an index to look up the combined output in a table
// containing actual combined waveform samples from OSC3.
// These samples are 8 bit, so 4 bits of waveform resolution is lost.
// All OSC3 samples are taken with FREQ=0x1000, adding a 1 to the upper 12
// bits of the accumulator each cycle for a sample period of 4096 cycles.
//
// Sawtooth+Triangle:
// The accumulator is used to look up an OSC3 sample.
//
// Pulse+Triangle:
// The accumulator is used to look up an OSC3 sample. When ring modulation is
// selected, the accumulator MSB is substituted with MSB EOR NOT sync_source MSB.
//
// Pulse+Sawtooth:
// The accumulator is used to look up an OSC3 sample.
// The sample is output if the pulse output is on.
//
// Pulse+Sawtooth+Triangle:
// The accumulator is used to look up an OSC3 sample.
// The sample is output if the pulse output is on.
//
// Combined waveforms including noise:
// All waveform combinations including noise output zero after a few cycles,
// since the waveform bits are and'ed into the shift register via the shift
// register outputs.

static reg12 NoisePulse6581(reg12 noise)
{
    return (noise < 0xf00) ? 0x000 : noise & (noise<<1) & (noise<<2);
}

static reg12 NoisePulse8580(reg12 noise)
{
    return (noise < 0xfc0) ? noise & (noise << 1) : 0xfc0;
}

inline void SID_WAVE::SetWaveformOutput()
{
    // Set output value.
    if (likely(waveform)) {
        // The bit masks no_pulse and no_noise are used to achieve branch-free
        // calculation of the output value.
        int ix = (accumulator ^ (~sync_source->accumulator & ring_msb_mask)) >> 12;

        waveform_output = wave[ix] & (no_pulse | pulse_output) & no_noise_or_noise_output;

        if (unlikely((waveform & 0xc) == 0xc))
        {
            waveform_output = (sid_model == MOS_6581) ?
                                  NoisePulse6581(waveform_output) : NoisePulse8580(waveform_output);
        }

        // Triangle/Sawtooth output is delayed half cycle on 8580.
        // This will appear as a one cycle delay on OSC3 as it is
        // latched in the first phase of the clock.
        if ((waveform & 3) && (sid_model == MOS_8580))
        {
            osc3 = tri_saw_pipeline & (no_pulse | pulse_output) & no_noise_or_noise_output;
            tri_saw_pipeline = wave[ix];
        }
        else
        {
            osc3 = waveform_output;
        }

        if ((waveform & 0x2) && unlikely(waveform & 0xd) && (sid_model == MOS_6581)) {
            // In the 6581 the top bit of the accumulator may be driven low by combined waveforms
            // when the sawtooth is selected
            accumulator &= (waveform_output << 12) | 0x7fffff;
        }

        if (unlikely(waveform > 0x8) && likely(!test) && likely(shift_pipeline != 1)) {
            // Combined waveforms write to the shift register.
            WriteShiftRegister();
        }
    }
    else {
        // Age floating DAC input.
        if (likely(floating_output_ttl) && unlikely(!--floating_output_ttl)) {
            WaveBitfade();
        }
    }

    // The pulse level is defined as (accumulator >> 12) >= pw ? 0xfff : 0x000.
    // The expression -((accumulator >> 12) >= pw) & 0xfff yields the same
    // results without any branching (and thus without any pipeline stalls).
    // NB! This expression relies on that the result of a boolean expression
    // is either 0 or 1, and furthermore requires two's complement integer.
    // A few more cycles may be saved by storing the pulse width left shifted
    // 12 bits, and dropping the and with 0xfff (this is valid since pulse is
    // used as a bit mask on 12 bit values), yielding the expression
    // -(accumulator >= pw24). However this only results in negligible savings.

    // The result of the pulse width compare is delayed one cycle.
    // Push next pulse level into pulse level pipeline.
    pulse_output = -((accumulator >> 12) >= pw) & 0xfff;
}

inline void SID_WAVE::SetWaveformOutput(cycle_count delta_t)
{
    // Set output value.
    if (likely(waveform)) {
        // The bit masks no_pulse and no_noise are used to achieve branch-free
        // calculation of the output value.
        int ix = (accumulator ^ (~sync_source->accumulator & ring_msb_mask)) >> 12;
        waveform_output =
            wave[ix] & (no_pulse | pulse_output) & no_noise_or_noise_output;
        // Triangle/Sawtooth output delay for the 8580 is not modeled
        osc3 = waveform_output;

        if ((waveform & 0x2) && unlikely(waveform & 0xd) && (sid_model == MOS_6581)) {
            accumulator &= (waveform_output << 12) | 0x7fffff;
        }

        if (unlikely(waveform > 0x8) && likely(!test)) {
            // Combined waveforms write to the shift register.
            // NB! Since cycles are skipped in delta_t clocking, writes will be
            // missed. Single cycle clocking must be used for 100% correct operation.
            WriteShiftRegister();
        }
    }
    else {
        if (likely(floating_output_ttl)) {
            // Age floating D/A output.
            floating_output_ttl -= delta_t;
            if (unlikely(floating_output_ttl <= 0)) {
                floating_output_ttl = 0;
                osc3 = waveform_output = 0;
            }
        }
    }
}


// ----------------------------------------------------------------------------
// Waveform output (12 bits).
// ----------------------------------------------------------------------------

// The digital waveform output is converted to an analog signal by a 12-bit
// DAC. Re-vectorized die photographs reveal that the DAC is an R-2R ladder
// built up as follows:
//
//        12V     11  10   9   8   7   6   5   4   3   2   1   0    GND
// Strange  |      |   |   |   |   |   |   |   |   |   |   |   |     |  Missing
// part    2R     2R  2R  2R  2R  2R  2R  2R  2R  2R  2R  2R  2R    2R  term.
// (bias)   |      |   |   |   |   |   |   |   |   |   |   |   |     |
//          --R-   --R---R---R---R---R---R---R---R---R---R---R--   ---
//                 |          _____
//               __|__     __|__   |
//               -----     =====   |
//               |   |     |   |   |
//        12V ---     -----     ------- GND
//                      |
//                     wout
//
// Bit on:  5V
// Bit off: 0V (GND)
//
// As is the case with all MOS 6581 DACs, the termination to (virtual) ground
// at bit 0 is missing. The MOS 8580 has correct termination, and has also
// done away with the bias part on the left hand side of the figure above.
//


short SID_WAVE::Output()
{
    // DAC imperfections are emulated by using waveform_output as an index
    // into a DAC lookup table. readOSC() uses waveform_output directly.
    return model_dac[sid_model][waveform_output];
}
