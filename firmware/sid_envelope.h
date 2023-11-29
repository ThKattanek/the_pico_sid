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

    enum State { ATTACK, DECAY_SUSTAIN, RELEASE, FREEZED };

    void SetSidType(sid_type type);
    void Clock(cycle_count delta_t);
    void Reset();

    void WriteControlReg(reg8 value);
    void WriteAttackDecay(reg8 value);
    void WriteSustainRelease(reg8 value);
    reg8 ReadEnv();
    reg8 OutWaveform();
    void StateChange();
    void SetExponentialCounter();

    // 8-bit envelope output.
    short Output();

protected:

    reg16 rate_counter;
    reg16 rate_period;
    reg8 exponential_counter;
    reg8 exponential_counter_period;
    reg8 new_exponential_counter_period;
    reg8 envelope_counter;
    reg8 env3;
    // Emulation of pipeline delay for envelope decrement.
    cycle_count envelope_pipeline;
    cycle_count exponential_pipeline;
    cycle_count state_pipeline;
    bool hold_zero;
    bool reset_rate_counter;

    reg4 attack;
    reg4 decay;
    reg4 sustain;
    reg4 release;

    reg8 gate;

    State state;
    State next_state;

    sid_type sid_model;

    // Lookup tables
    static reg16 rate_counter_period[];
    static reg8 sustain_level[];
    static unsigned short model_dac[2][1 << 8];

    friend class PICO_SID;
};

inline void SID_ENVELOPE::Clock(cycle_count delta_t)
{
    // NB! Any pipelined envelope counter decrement from single cycle clocking
    // will be lost. It is not worth the trouble to flush the pipeline here.

    if (unlikely(state_pipeline)) {
        if (next_state == ATTACK) {
            state = ATTACK;
            hold_zero = false;
            rate_period = rate_counter_period[attack];
        } else if (next_state == RELEASE) {
            state = RELEASE;
            rate_period = rate_counter_period[release];
        } else if (next_state == FREEZED) {
            hold_zero = true;
        }
        state_pipeline = 0;
    }

    // Check for ADSR delay bug.
    // If the rate counter comparison value is set below the current value of the
    // rate counter, the counter will continue counting up until it wraps around
    // to zero at 2^15 = 0x8000, and then count rate_period - 1 before the
    // envelope can finally be stepped.
    // This has been verified by sampling ENV3.
    //

    // NB! This requires two's complement integer.
    int rate_step = rate_period - rate_counter;
    if (unlikely(rate_step <= 0)) {
        rate_step += 0x7fff;
    }

    while (delta_t) {
        if (delta_t < rate_step) {
            // likely (~65%)
            rate_counter += delta_t;
            if (unlikely(rate_counter & 0x8000)) {
                ++rate_counter &= 0x7fff;
            }
            return;
        }

        rate_counter = 0;
        delta_t -= rate_step;

        // The first envelope step in the attack state also resets the exponential
        // counter. This has been verified by sampling ENV3.
        //
        if (state == ATTACK || ++exponential_counter == exponential_counter_period) {
            // likely (~50%)
            exponential_counter = 0;

            // Check whether the envelope counter is frozen at zero.
            if (unlikely(hold_zero)) {
                rate_step = rate_period;
                continue;
            }

            switch (state) {
            case ATTACK:
                // The envelope counter can flip from 0xff to 0x00 by changing state to
                // release, then to attack. The envelope counter is then frozen at
                // zero; to unlock this situation the state must be changed to release,
                // then to attack. This has been verified by sampling ENV3.
                //
                ++envelope_counter &= 0xff;
                if (unlikely(envelope_counter == 0xff)) {
                    state = DECAY_SUSTAIN;
                    rate_period = rate_counter_period[decay];
                }
                break;
            case DECAY_SUSTAIN:
                if (likely(envelope_counter != sustain_level[sustain])) {
                    --envelope_counter;
                }
                break;
            case RELEASE:
                // The envelope counter can flip from 0x00 to 0xff by changing state to
                // attack, then to release. The envelope counter will then continue
                // counting down in the release state.
                // This has been verified by sampling ENV3.
                // NB! The operation below requires two's complement integer.
                //
                --envelope_counter &= 0xff;
                break;
            case FREEZED:
                // we should never get here
                break;
            }

            // Check for change of exponential counter period.
            SetExponentialCounter();
            if (unlikely(new_exponential_counter_period > 0)) {
                exponential_counter_period = new_exponential_counter_period;
                new_exponential_counter_period = 0;
                if (next_state == FREEZED) {
                    hold_zero = true;
                }
            }
        }

        rate_step = rate_period;
    }
}

inline void SID_ENVELOPE::StateChange()
{
    state_pipeline--;

    switch (next_state) {
    case ATTACK:
        if (state_pipeline == 0) {
            state = ATTACK;
            // The attack register is correctly activated during second cycle of attack phase
            rate_period = rate_counter_period[attack];
            hold_zero = false;
        }
        break;
    case DECAY_SUSTAIN:
        break;
    case RELEASE:
        if (((state == ATTACK) && (state_pipeline == 0))
            || ((state == DECAY_SUSTAIN) && (state_pipeline == 1))) {
            state = RELEASE;
            rate_period = rate_counter_period[release];
        }
        break;
    case FREEZED:
        break;
    }
}

inline void SID_ENVELOPE::SetExponentialCounter()
{
    // Check for change of exponential counter period.
    switch (envelope_counter) {
    case 0xff:
        exponential_counter_period = 1;
        break;
    case 0x5d:
        exponential_counter_period = 2;
        break;
    case 0x36:
        exponential_counter_period = 4;
        break;
    case 0x1a:
        exponential_counter_period = 8;
        break;
    case 0x0e:
        exponential_counter_period = 16;
        break;
    case 0x06:
        exponential_counter_period = 30;
        break;
    case 0x00:
        // TODO write a test to verify that 0x00 really changes the period
        // e.g. set R = 0xf, gate on to 0x06, gate off to 0x00, gate on to 0x04,
        // gate off, sample.
        exponential_counter_period = 1;

        // When the envelope counter is changed to zero, it is frozen at zero.
        // This has been verified by sampling ENV3.
        hold_zero = true;
        break;
    }
}

inline short SID_ENVELOPE::Output()
{
    // DAC imperfections are emulated by using envelope_counter as an index
    // into a DAC lookup table. readENV() uses envelope_counter directly.
    return model_dac[sid_model][envelope_counter];
}

#endif // SID_ENVELOPE_CLASS_H
