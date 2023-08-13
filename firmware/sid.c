//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid.c                                 //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
//////////////////////////////////////////////////

#include "sid.h"

inline void SidInit(SID *sid)
{
	sid->Osc[0].OscSource = &(sid->Osc[2]);
	sid->Osc[1].OscSource = &(sid->Osc[0]);
	sid->Osc[2].OscSource = &(sid->Osc[1]);

	sid->Osc[0].OscDestination = &(sid->Osc[1]);
	sid->Osc[1].OscDestination = &(sid->Osc[2]);
	sid->Osc[2].OscDestination = &(sid->Osc[0]);

	SidReset(sid);
}

inline void SidReset(SID *sid)
{
    sid->Freq0Lo = 0;
    sid->Freq0Hi = 0;
    sid->Freq1Lo = 0;
    sid->Freq1Hi = 0;
    sid->Freq2Lo = 0;
    sid->Freq2Hi = 0;
    sid->Puls0Lo = 0;
    sid->Puls0Hi = 0;
    sid->Puls1Lo = 0;
    sid->Puls1Hi = 0;
    sid->Puls2Lo = 0;
    sid->Puls2Hi = 0;
    sid->Ctrl0 = 0;
    sid->Ctrl1 = 0;
    sid->Ctrl2 = 0;

    sid->FilterFreqLo = 0;
    sid->FilterFreqHi = 0;

	sid->VolumeOut = 0.0f;

	OscReset(&sid->Osc[0]);
	OscReset(&sid->Osc[1]);
	OscReset(&sid->Osc[2]);

	EnvReset(&sid->Env[0]);
	EnvReset(&sid->Env[1]);
	EnvReset(&sid->Env[2]);

    //Filter->Reset();	
}

inline void SidWriteReg(SID *sid, uint8_t address, uint8_t value)
{	
    sid->IO[address & 31] = value;

    switch(address & 0x1F)
    {
    case 0: // Oszillatorfrequenz LoByte Stimme 1
        sid->Freq0Lo = value;
		OscSetFrequenz(&sid->Osc[0], (sid->Freq0Hi << 8) | sid->Freq0Lo);
        break;
    case 1: // Oszillatorfrequenz HiByte Stimme 1
        sid->Freq0Hi = value;
        OscSetFrequenz(&sid->Osc[0], (sid->Freq0Hi << 8) | sid->Freq0Lo);
        break;
    case 2: // Pulsweite LoByte Stimme 1
        sid->Puls0Lo = value;
		OscSetPulesCompare(&sid->Osc[0], (sid->Puls0Hi << 8) | sid->Puls0Lo);
        break;
    case 3: // Pulsweite HiByte Stimme 1
        sid->Puls0Hi = value;
        OscSetPulesCompare(&sid->Osc[0], (sid->Puls0Hi << 8) | sid->Puls0Lo);
        break;
    case 4: // Steuerregister Stimme 1
        sid->Ctrl0 = value;
		OscSetControlBits(&sid->Osc[0], sid->Ctrl0);
		EnvSetKeyBit(&sid->Env[0], value & 0x01);
        break;
    case 5: // Attack & Decay Stimme 1
        EnvSetAttackDecay(&sid->Env[0], value);
        break;
    case 6: // Sustain & Release Stimme 1
        EnvSetSustainRelease(&sid->Env[0], value);
        break;
    case 7: // Oszillatorfrequenz LoByte Stimme 2
        sid->Freq1Lo = value;
        OscSetFrequenz(&sid->Osc[1], (sid->Freq1Hi << 8) | sid->Freq1Lo);
        break;
    case 8: // Oszillatorfrequenz HiByte Stimme 2
        sid->Freq1Hi = value;
		OscSetFrequenz(&sid->Osc[1], (sid->Freq1Hi << 8) | sid->Freq1Lo);       
     	break;
    case 9: // Pulsweite LoByte Stimme 2
        sid->Puls1Lo = value;
        OscSetPulesCompare(&sid->Osc[1], (sid->Puls1Hi << 8) | sid->Puls1Lo);
        break;
    case 10: // Pulsweite HiByte Stimme 2
        sid->Puls1Hi = value;
        OscSetPulesCompare(&sid->Osc[1], (sid->Puls1Hi << 8) | sid->Puls1Lo);
        break;
    case 11: // Steuerregister Stimme 2
        sid->Ctrl1 = value;
        OscSetControlBits(&sid->Osc[1], sid->Ctrl1);
		EnvSetKeyBit(&sid->Env[1], value & 0x01);
        break;
    case 12: // Attack & Decay Stimme 2
		EnvSetAttackDecay(&sid->Env[1], value);
        break;
    case 13: // Sustain & Release Stimme 2
        EnvSetSustainRelease(&sid->Env[1], value);
        break;
    case 14: // Oszillatorfrequenz LoByte Stimme 3
        sid->Freq2Lo = value;
        OscSetFrequenz(&sid->Osc[2], (sid->Freq2Hi << 8) | sid->Freq2Lo);
        break;
    case 15: // Oszillatorfrequenz HiByte Stimme 3
        sid->Freq2Hi = value;
        OscSetFrequenz(&sid->Osc[2], (sid->Freq2Hi << 8) | sid->Freq2Lo);
        break;
    case 16: // Pulsweite LoByte Stimme 3
        sid->Puls2Lo = value;
        OscSetPulesCompare(&sid->Osc[2], (sid->Puls2Hi << 8) | sid->Puls2Lo);
        break;
    case 17: // Pulsweite HiByte Stimme 3
        sid->Puls2Hi = value;
        OscSetPulesCompare(&sid->Osc[2], (sid->Puls2Hi << 8) | sid->Puls2Lo);
        break;
    case 18: // Steuerregister Stimme 3
        sid->Ctrl2 = value;
        OscSetControlBits(&sid->Osc[2], sid->Ctrl2);
		EnvSetKeyBit(&sid->Env[2], value & 0x01);
        break;
    case 19: // Attack & Decay Stimme 3
        EnvSetAttackDecay(&sid->Env[2], value);
        break;
    case 20: // Sustain & Release Stimme 3
		EnvSetSustainRelease(&sid->Env[2], value);
        break;
    case 21: // Filterfrequenz LoByte
        sid->FilterFreqLo = value;
        //Filter->SetFrequenz((FilterFreqHi<<3) | (FilterFreqLo & 0x07));
        break;
    case 22: // Filterfrequenz HByte
        sid->FilterFreqHi = value;
        //Filter->SetFrequenz((FilterFreqHi<<3) | (FilterFreqLo & 0x07));
        break;
    case 23: // FilterControl1
        //Filter->SetControl1(value);
        break;
    case 24: // FilterControl2
        //Filter->SetControl2(value);
		sid->VolumeOut = (value & 0x0f) / (float)0x0f;
        break;
    default:
        break;
    }
}
