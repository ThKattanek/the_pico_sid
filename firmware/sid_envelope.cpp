//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid_envelope.cpp                      //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
// The template used was resid:                 //
// https://github.com/libsidplayfp/resid        //
//                                              //
//////////////////////////////////////////////////

#include "./sid_envelope.h"
#include "./sid_dac.h"

reg16 SID_ENVELOPE::rate_counter_period[] = {
    8,  //   2ms*1.0MHz/256 =     7.81
    31,  //   8ms*1.0MHz/256 =    31.25
    62,  //  16ms*1.0MHz/256 =    62.50
    94,  //  24ms*1.0MHz/256 =    93.75
    148,  //  38ms*1.0MHz/256 =   148.44
    219,  //  56ms*1.0MHz/256 =   218.75
    266,  //  68ms*1.0MHz/256 =   265.63
    312,  //  80ms*1.0MHz/256 =   312.50
    391,  // 100ms*1.0MHz/256 =   390.63
    976,  // 250ms*1.0MHz/256 =   976.56
    1953,  // 500ms*1.0MHz/256 =  1953.13
    3125,  // 800ms*1.0MHz/256 =  3125.00
    3906,  //   1 s*1.0MHz/256 =  3906.25
    11719,  //   3 s*1.0MHz/256 = 11718.75
    19531,  //   5 s*1.0MHz/256 = 19531.25
    31250   //   8 s*1.0MHz/256 = 31250.00
};

reg8 SID_ENVELOPE::sustain_level[] = {
    0x00,
    0x11,
    0x22,
    0x33,
    0x44,
    0x55,
    0x66,
    0x77,
    0x88,
    0x99,
    0xaa,
    0xbb,
    0xcc,
    0xdd,
    0xee,
    0xff,
};

// DAC lookup tables.
unsigned short SID_ENVELOPE::model_dac[2][1 << 8] = {
    {0},
    {0},
    };


SID_ENVELOPE::SID_ENVELOPE()
{
    static bool class_init;

    if (!class_init)
    {
        // Build DAC lookup tables for 8-bit DACs.
        // MOS 6581: 2R/R ~ 2.20, missing termination resistor.
        BuildDacTable(model_dac[0], 8, 2.20, false);
        // MOS 8580: 2R/R ~ 2.00, correct termination.
        BuildDacTable(model_dac[1], 8, 2.00, true);

        class_init = true;
    }

    SetSidType(MOS_6581);

    // Counter's odd bits are high on powerup
    envelope_counter = 0xaa;

    // just to avoid uninitialized access with delta clocking
    next_state = RELEASE;

    Reset();
}

SID_ENVELOPE::~SID_ENVELOPE()
{

}

void SID_ENVELOPE::SetSidType(sid_type type)
{
    sid_model = type;
}

void SID_ENVELOPE::Reset()
{
    envelope_pipeline = 0;
    exponential_pipeline = 0;

    state_pipeline = 0;

    attack = 0;
    decay = 0;
    sustain = 0;
    release = 0;

    gate = 0;

    rate_counter = 0;
    exponential_counter = 0;
    exponential_counter_period = 1;
    new_exponential_counter_period = 0;
    reset_rate_counter = false;

    state = RELEASE;
    rate_period = rate_counter_period[release];
    hold_zero = false;
}

void SID_ENVELOPE::WriteControlReg(reg8 value)
{
    reg8 gate_next = value & 0x01;

    if (gate != gate_next)
    {
        next_state = gate_next ? ATTACK : RELEASE;
        if (next_state == ATTACK)
        {
            state = DECAY_SUSTAIN;
            rate_period = rate_counter_period[decay];
            state_pipeline = 2;
            if (reset_rate_counter || exponential_pipeline == 2)
            {
                envelope_pipeline = exponential_counter_period == 1 || exponential_pipeline == 2 ? 2 : 4;
            }
            else if (exponential_pipeline == 1) { state_pipeline = 3; }
        }
        else {state_pipeline = envelope_pipeline > 0 ? 3 : 2;}
        gate = gate_next;
    }
}

void SID_ENVELOPE::WriteAttackDecay(reg8 value)
{
    attack = (value >> 4) & 0x0f;
    decay = value & 0x0f;
    if (state == ATTACK)
    {
        rate_period = rate_counter_period[attack];
    }
    else if (state == DECAY_SUSTAIN)
    {
        rate_period = rate_counter_period[decay];
    }
}

void SID_ENVELOPE::WriteSustainRelease(reg8 value)
{
    sustain = (value >> 4) & 0x0f;
    release = value & 0x0f;
    if (state == RELEASE)
    {
        rate_period = rate_counter_period[release];
    }
}

reg8 SID_ENVELOPE::ReadEnv()
{
    return env3;
}

reg8 SID_ENVELOPE::OutWaveform()
{
    return envelope_counter;
}

RESID_INLINE
void SID_ENVELOPE::Clock(cycle_count delta_t)
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

RESID_INLINE
void SID_ENVELOPE::StateChange()
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

RESID_INLINE
void SID_ENVELOPE::SetExponentialCounter()
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

RESID_INLINE
short SID_ENVELOPE::Output()
{
    // DAC imperfections are emulated by using envelope_counter as an index
    // into a DAC lookup table. readENV() uses envelope_counter directly.
    return model_dac[sid_model][envelope_counter];
}
