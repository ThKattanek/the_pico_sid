//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: env.h                                 //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
//////////////////////////////////////////////////

#ifndef ENV_H
#define ENV_H

#include <pico/stdlib.h>

#define	ATTACK			0
#define	DECAY_SUSTAIN   1
#define RELEASE			2

static const uint16_t RateCounterPeriod[]={9,32,63,95,149,220,267,313,392,977,1954,3126,3907,11720,19532,31251};
static const uint8_t SustainLevel[]={0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};

struct ENV
{
	bool		KeyBit;
    uint8_t		State;
    uint32_t	RatePeriod;
    uint32_t	RateCounter;
    uint32_t	ExponentialCounterPeriod;
    uint32_t	ExponentialCounter;
    uint32_t	EnvCounter;
    bool		HoldZero;
    uint32_t	Attack;
    uint32_t	Decay;
    uint32_t	Sustain;
    uint32_t	Release;
}typedef ENV;

void EnvReset(ENV* env);
void EnvSetKeyBit(ENV* env, bool value);
void EnvSetAttackDecay(ENV* env, uint8_t value);
void EnvSetSustainRelease(ENV* env, uint8_t value);
uint8_t EnvGetOutput(ENV* env);
void EnvOneCycle(ENV* env);
void EnvExecuteCycles(ENV* env, uint8_t cycles);

#endif // ENV_H