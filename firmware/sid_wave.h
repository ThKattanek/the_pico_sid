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

// ----------------------------------------------------------------------------
// SID clocking - delta_t cycles.
// ----------------------------------------------------------------------------

inline void SID_WAVE::Clock(cycle_count delta_t)
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

inline short SID_WAVE::Output()
{
    // DAC imperfections are emulated by using waveform_output as an index
    // into a DAC lookup table. readOSC() uses waveform_output directly.
    return model_dac[sid_model][waveform_output];
}

#endif // SID_WAVE_CLASS_H
