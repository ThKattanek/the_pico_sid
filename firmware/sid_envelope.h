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

#endif // SID_ENVELOPE_CLASS_H
