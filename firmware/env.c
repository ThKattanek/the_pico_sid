//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: env.c                                 //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
//////////////////////////////////////////////////

#include "env.h"

inline void EnvReset(ENV* env)
{
	env->EnvCounter = 0;
    env->Attack = 0;
    env->Decay = 0;
    env->Sustain = 0;
    env->Release = 0;
    env->KeyBit = false;
    env->RateCounter = 0;
    env->State = RELEASE;
    env->RatePeriod = RateCounterPeriod[env->Release];
    env->HoldZero=true;
    env->ExponentialCounter = 0;
    env->ExponentialCounterPeriod = 1;
}

inline void EnvSetKeyBit(ENV* env, bool value)
{
    if (!env->KeyBit && value)
    {
        env->State = ATTACK;
        env->RatePeriod = RateCounterPeriod[env->Attack];
        env->HoldZero = false;
    }
    else if (env->KeyBit && !value)
    {
        env->State = RELEASE;
        env->RatePeriod = RateCounterPeriod[env->Release];
    }
    env->KeyBit = value;
}

inline void EnvSetAttackDecay(ENV* env, uint8_t value)
{
    env->Attack = (value >> 4) & 0x0f;
    env->Decay = value & 0x0f;
    if(env->State == ATTACK) 
		env->RatePeriod = RateCounterPeriod[env->Attack];
    else if 
		(env->State == DECAY_SUSTAIN) env->RatePeriod = RateCounterPeriod[env->Decay];
}

inline void EnvSetSustainRelease(ENV* env, uint8_t value)
{
    env->Sustain = (value >> 4) & 0x0f;
    env->Release = value & 0x0f;
    if(env->State == RELEASE) 
		env->RatePeriod = RateCounterPeriod[env->Release];
}

inline uint8_t EnvGetOutput(ENV* env)
{
	return env->EnvCounter;
}

inline void EnvOneCycle(ENV* env)
{
	env->RateCounter++;
	if(env->RateCounter & 0x8000) 
	{
		env->RateCounter++;
		env->RateCounter &= 0x7fff;
	}

	if(env->RateCounter == env->RatePeriod)
	{
        env->RateCounter=0;

        if(env->State == ATTACK || ++env->ExponentialCounter == env->ExponentialCounterPeriod)
        {
            env->ExponentialCounter = 0;

            if(!env->HoldZero)
            {
                switch(env->State)
                {
                case ATTACK:
					env->EnvCounter++;
                    env->EnvCounter &= 0xff;
                    if(env->EnvCounter == 0xff)
                    {
                        env->State = DECAY_SUSTAIN;
                        env->RatePeriod = RateCounterPeriod[env->Decay];
                    }
                    break;

                case DECAY_SUSTAIN:
                    if(env->EnvCounter != SustainLevel[env->Sustain])
					{
						env->EnvCounter--;
					}
                    break;

                case RELEASE:
					env->EnvCounter--;
                    env->EnvCounter &= 0xff;
                    break;
                }

                switch(env->EnvCounter)
                {
                case 0xFF:
                    env->ExponentialCounterPeriod = 1;
                    break;

                case 0x5D:
                    env->ExponentialCounterPeriod = 2;
                    break;

                case 0x36:
                    env->ExponentialCounterPeriod = 4;
                    break;

                case 0x1A:
                    env->ExponentialCounterPeriod = 8;
                    break;

                case 0x0E:
                    env->ExponentialCounterPeriod = 16;
                    break;

                case 0x06:
                    env->ExponentialCounterPeriod = 30;
                    break;

                case 0x00:
                    env->ExponentialCounterPeriod = 1;
                    env->HoldZero = true;
                    break;
                }
            }
        }
    }
}

inline void EnvExecuteCycles(ENV* env, uint8_t cycles)
{
	///////////////////// VOICE 1 //////////////////// 

	env->RateCounter += cycles;
	if(env->RateCounter & 0x8000) 
	{
		env->RateCounter += cycles;
		env->RateCounter &= 0x7fff;
	}

	if(env->RateCounter >= env->RatePeriod)
	{
        env->RateCounter -= env->RateCounter;

		env->ExponentialCounter++;
        if(env->State == ATTACK || env->ExponentialCounter == env->ExponentialCounterPeriod)
        {
            env->ExponentialCounter = 0;

            if(!env->HoldZero)
            {
                switch(env->State)
                {
                case ATTACK:
					env->EnvCounter++;
                    env->EnvCounter &= 0xff;
                    if(env->EnvCounter == 0xff)
                    {
                        env->State = DECAY_SUSTAIN;
                        env->RatePeriod = RateCounterPeriod[env->Decay];
                    }
                    break;

                case DECAY_SUSTAIN:
                    if(env->EnvCounter != SustainLevel[env->Sustain])
					{
						env->EnvCounter--;
					}
                    break;

                case RELEASE:
					env->EnvCounter--;
                    env->EnvCounter &= 0xff;
                    break;
                }

                switch(env->EnvCounter)
                {
                case 0xFF:
                    env->ExponentialCounterPeriod = 1;
                    break;

                case 0x5D:
                    env->ExponentialCounterPeriod = 2;
                    break;

                case 0x36:
                    env->ExponentialCounterPeriod = 4;
                    break;

                case 0x1A:
                    env->ExponentialCounterPeriod = 8;
                    break;

                case 0x0E:
                    env->ExponentialCounterPeriod = 16;
                    break;

                case 0x06:
                    env->ExponentialCounterPeriod = 30;
                    break;

                case 0x00:
                    env->ExponentialCounterPeriod = 1;
                    env->HoldZero = true;
                    break;
                }
            }
        }
    }

	///////////////////// VOICE 2 //////////////////// 

	env++;
	env->RateCounter += cycles;
	if(env->RateCounter & 0x8000) 
	{
		env->RateCounter += cycles;
		env->RateCounter &= 0x7fff;
	}

	if(env->RateCounter >= env->RatePeriod)
	{
        env->RateCounter -= env->RateCounter;

		env->ExponentialCounter++;
        if(env->State == ATTACK || env->ExponentialCounter == env->ExponentialCounterPeriod)
        {
            env->ExponentialCounter = 0;

            if(!env->HoldZero)
            {
                switch(env->State)
                {
                case ATTACK:
					env->EnvCounter++;
                    env->EnvCounter &= 0xff;
                    if(env->EnvCounter == 0xff)
                    {
                        env->State = DECAY_SUSTAIN;
                        env->RatePeriod = RateCounterPeriod[env->Decay];
                    }
                    break;

                case DECAY_SUSTAIN:
                    if(env->EnvCounter != SustainLevel[env->Sustain])
					{
						env->EnvCounter--;
					}
                    break;

                case RELEASE:
					env->EnvCounter--;
                    env->EnvCounter &= 0xff;
                    break;
                }

                switch(env->EnvCounter)
                {
                case 0xFF:
                    env->ExponentialCounterPeriod = 1;
                    break;

                case 0x5D:
                    env->ExponentialCounterPeriod = 2;
                    break;

                case 0x36:
                    env->ExponentialCounterPeriod = 4;
                    break;

                case 0x1A:
                    env->ExponentialCounterPeriod = 8;
                    break;

                case 0x0E:
                    env->ExponentialCounterPeriod = 16;
                    break;

                case 0x06:
                    env->ExponentialCounterPeriod = 30;
                    break;

                case 0x00:
                    env->ExponentialCounterPeriod = 1;
                    env->HoldZero = true;
                    break;
                }
            }
        }
    }

	///////////////////// VOICE 3 //////////////////// 

	env++;
	env->RateCounter += cycles;
	if(env->RateCounter & 0x8000) 
	{
		env->RateCounter += cycles;
		env->RateCounter &= 0x7fff;
	}

	if(env->RateCounter >= env->RatePeriod)
	{
        env->RateCounter -= env->RateCounter;

		env->ExponentialCounter++;
        if(env->State == ATTACK || env->ExponentialCounter == env->ExponentialCounterPeriod)
        {
            env->ExponentialCounter = 0;

            if(!env->HoldZero)
            {
                switch(env->State)
                {
                case ATTACK:
					env->EnvCounter++;
                    env->EnvCounter &= 0xff;
                    if(env->EnvCounter == 0xff)
                    {
                        env->State = DECAY_SUSTAIN;
                        env->RatePeriod = RateCounterPeriod[env->Decay];
                    }
                    break;

                case DECAY_SUSTAIN:
                    if(env->EnvCounter != SustainLevel[env->Sustain])
					{
						env->EnvCounter--;
					}
                    break;

                case RELEASE:
					env->EnvCounter--;
                    env->EnvCounter &= 0xff;
                    break;
                }

                switch(env->EnvCounter)
                {
                case 0xFF:
                    env->ExponentialCounterPeriod = 1;
                    break;

                case 0x5D:
                    env->ExponentialCounterPeriod = 2;
                    break;

                case 0x36:
                    env->ExponentialCounterPeriod = 4;
                    break;

                case 0x1A:
                    env->ExponentialCounterPeriod = 8;
                    break;

                case 0x0E:
                    env->ExponentialCounterPeriod = 16;
                    break;

                case 0x06:
                    env->ExponentialCounterPeriod = 30;
                    break;

                case 0x00:
                    env->ExponentialCounterPeriod = 1;
                    env->HoldZero = true;
                    break;
                }
            }
        }
    }
}