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
