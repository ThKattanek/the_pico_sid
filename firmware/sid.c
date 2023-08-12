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
#include "waves.h"

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

	OscReset(&sid->Osc[0]);
	OscReset(&sid->Osc[1]);
	OscReset(&sid->Osc[2]);

    //sid->osc[0]->Reset();
    //sid->osc[1]->Reset();
    //sid->osc[2]->Reset();

    //ENV[0]->Reset();
    //ENV[1]->Reset();
    //ENV[2]->Reset();

    //Filter->Reset();	
}

inline void OscReset(OSC *osc)
{
    osc->FrequenzCounter = 0;
    osc->ShiftRegister = 0x7FFFF8;
    osc->FrequenzAdd = 0;
    osc->PulseCompare = 0;
    osc->WaveForm = 0;
    osc->TestBit = false;
    osc->RingBit = false;
    osc->SyncBit = false;
}

inline void OscSetFrequenz(OSC *osc, uint16_t frequency)
{
	osc->FrequenzAdd = frequency;
}

inline void OscSetControlBits(OSC *osc, uint8_t ctrlbits)
{
    osc->WaveForm = ctrlbits>>4;

    if(ctrlbits & 0x08)
    {
        osc->FrequenzCounter = 0;
    	osc->ShiftRegister = 0;
    }

    else if(osc->TestBit) osc->ShiftRegister = 0x7FFFF8;
    osc->TestBit = (ctrlbits & 0x08) >> 3;
    osc->RingBit = (ctrlbits & 0x04) >> 2;
    osc->SyncBit = (ctrlbits & 0x02) >> 1;	
}

void OscSetPulesCompare(OSC *osc, uint16_t pulsecompare)
{
    osc->PulseCompare = pulsecompare & 0xFFF;
}

inline uint16_t OscGetDreieck(OSC *osc)
{
    unsigned int MSB;
	/*
    if((osc->RingBit == true) && (osc->OSCSource != 0))
        MSB = (osc->FrequenzCounter ^ osc->OSCSource->FrequenzCounter) & 0x800000;

    else*/ MSB = osc->FrequenzCounter & 0x800000;

    return ((MSB)?(~osc->FrequenzCounter>>11):(osc->FrequenzCounter>>11)) & 0xFFF;
}

inline uint16_t OscGetOutput(OSC *osc)
{
    if(osc->WaveForm == 0) return 0;

    switch(osc->WaveForm)
    {
    /// Die 4 Grundwellenformen ///
    case 1: // Dreieck
        return OscGetDreieck(osc);
    case 2: // Saegezahn
        return osc->FrequenzCounter>>12;
    case 3: // Mischform Saegezahn/Dreieck (Wave0)
        return wave0[osc->FrequenzCounter>>12]<<4;
    case 4: // Rechteck
        if(osc->PulseCompare == 0xFFF) return 0xFFF;
        return ((osc->FrequenzCounter>>12) >= osc->PulseCompare)?0xFFF:0x000;
    case 5: // Mischform
        return ((wave1[OscGetDreieck(osc)>>1]<<4) & (((osc->FrequenzCounter>>12) >= osc->PulseCompare)?0xFFF:0x000));;
    case 6: // Mischform S�gezahn/Dreieck
        return ((wave2[osc->FrequenzCounter>>12]<<4) & (((osc->FrequenzCounter>>12) >= osc->PulseCompare)?0xFFF:0x000));
    case 7: // Mischform S�gezahn/Dreieck
        return ((wave3[osc->FrequenzCounter>>12]<<4) & (((osc->FrequenzCounter>>12) >= osc->PulseCompare)?0xFFF:0x000));
    case 8: // Rauschen
        return ((osc->ShiftRegister&0x400000)>>11)|((osc->ShiftRegister&0x100000)>>10)|((osc->ShiftRegister&0x010000)>>7)|((osc->ShiftRegister&0x002000)>>5)|((osc->ShiftRegister&0x000800)>>4)|((osc->ShiftRegister&0x000080)>>1)|((osc->ShiftRegister&0x000010)<<1)|((osc->ShiftRegister&0x000004)<< 2);
    /// Mischwellenformen ///
    default:
        return 0;
    }
}

inline void OscOneCycle(OSC *osc)
{
    if(!osc->TestBit)
    {
        osc->FrequenzCounterOld = osc->FrequenzCounter;
        osc->FrequenzCounter += osc->FrequenzAdd;
        osc->FrequenzCounter &= 0xFFFFFF;
    }

    osc->FrequenzCounterMsbRising = !(osc->FrequenzCounterOld&0x800000)&&(osc->FrequenzCounter&0x800000);

    if(!(osc->FrequenzCounterOld&0x080000)&&(osc->FrequenzCounter&0x080000))
    {
        osc->Bit0 = ((osc->ShiftRegister>>22)^(osc->ShiftRegister>>17))&0x01;
        osc->ShiftRegister = ((osc->ShiftRegister<<1) & 0x7FFFFF) | osc->Bit0;
    }


    /// Oscilltor mit Source Sycronisieren ?? ///
	/*
    if (FrequenzCounterMsbRising && OSCDestination->SyncBit && !(SyncBit && OSCSource->FrequenzCounterMsbRising))
    {
        OSCDestination->FrequenzCounter = 0;
    }
	*/
}

inline void OscExecuteCycles(OSC *osc, uint8_t cycles)
{
    if(!osc->TestBit)
    {
        osc->FrequenzCounterOld = osc->FrequenzCounter;
        osc->FrequenzCounter += osc->FrequenzAdd * cycles;
        osc->FrequenzCounter &= 0xFFFFFF;
    }

    osc->FrequenzCounterMsbRising = !(osc->FrequenzCounterOld&0x800000)&&(osc->FrequenzCounter&0x800000);

    if(!(osc->FrequenzCounterOld&0x080000)&&(osc->FrequenzCounter&0x080000))
    {
        osc->Bit0 = ((osc->ShiftRegister>>22)^(osc->ShiftRegister>>17))&0x01;
        osc->ShiftRegister = ((osc->ShiftRegister<<1) & 0x7FFFFF) | osc->Bit0;
    }


    /// Oscilltor mit Source Sycronisieren ?? ///
	/*
    if (FrequenzCounterMsbRising && OSCDestination->SyncBit && !(SyncBit && OSCSource->FrequenzCounterMsbRising))
    {
        OSCDestination->FrequenzCounter = 0;
    }
	*/
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
        //ENV[0]->SetKeyBit(value & 0x01);
        break;
    case 5: // Attack & Decay Stimme 1
        //ENV[0]->SetAttackDecay(value);
        break;
    case 6: // Sustain & Release Stimme 1
        //ENV[0]->SetSustainRelease(value);
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
        //ENV[1]->SetKeyBit(wert & 0x01);
        break;
    case 12: // Attack & Decay Stimme 2
        //ENV[1]->SetAttackDecay(value);
        break;
    case 13: // Sustain & Release Stimme 2
        //ENV[1]->SetSustainRelease(value);
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
        //ENV[2]->SetKeyBit(wert & 0x01);
        break;
    case 19: // Attack & Decay Stimme 2
        //ENV[2]->SetAttackDecay(value);
        break;
    case 20: // Sustain & Release Stimme 2
        //ENV[2]->SetSustainRelease(value);
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
        break;
    default:
        break;
    }
}
