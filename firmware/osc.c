//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: osc.c                                 //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
//////////////////////////////////////////////////

#include "osc.h"
#include "waves.h"

inline void OscReset(OSC* osc)
{
    osc->FrequenzCounter = 0;
	osc->FrequenzCounterOld = 0;
    osc->ShiftRegister = 0x7ffff8;
    osc->FrequenzAdd = 0;
    osc->PulseCompare = 0;
    osc->WaveForm = 0;
    osc->TestBit = false;
    osc->RingBit = false;
    osc->SyncBit = false;
}

inline void OscSetFrequenz(OSC* osc, uint16_t frequency)
{
	osc->FrequenzAdd = frequency;
}

inline void OscSetControlBits(OSC* osc, uint8_t ctrlbits)
{
    osc->WaveForm = ctrlbits >> 4;

    if(ctrlbits & 0x08)
    {
        osc->FrequenzCounter = 0;
    	osc->ShiftRegister = 0;
    }
    else if(osc->TestBit) 
		osc->ShiftRegister = 0x7FFFF8;

    osc->TestBit = (ctrlbits & 0x08) >> 3;
    osc->RingBit = (ctrlbits & 0x04) >> 2;
    osc->SyncBit = (ctrlbits & 0x02) >> 1;	
}

inline void OscSetPulesCompare(OSC* osc, uint16_t pulsecompare)
{
    osc->PulseCompare = pulsecompare & 0xfff;
}

inline uint16_t OscGetDreieck(OSC* osc)
{
    unsigned int MSB;
	OSC* osc_source = osc->OscSource;
#ifndef DISABLE_RINGMOD
    if(osc->RingBit == true)
        MSB = (osc->FrequenzCounter ^ osc_source->FrequenzCounter) & 0x800000;
    else
#endif
		MSB = osc->FrequenzCounter & 0x800000;

    return ((MSB)?(~osc->FrequenzCounter >> 11) : (osc->FrequenzCounter >> 11)) & 0xfff;
}

inline uint16_t OscGetOutput(OSC* osc)
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

inline void OscOneCycle(OSC* osc)
{
    if(!osc->TestBit)
    {
        osc->FrequenzCounterOld = osc->FrequenzCounter;
        osc->FrequenzCounter += osc->FrequenzAdd;
        osc->FrequenzCounter &= 0xffffff;
    }

    osc->FrequenzCounterMsbRising = !(osc->FrequenzCounterOld & 0x800000) && (osc->FrequenzCounter & 0x800000);

    if(!(osc->FrequenzCounterOld & 0x080000) && (osc->FrequenzCounter & 0x080000))
    {
        osc->Bit0 = ((osc->ShiftRegister >> 22) ^ (osc->ShiftRegister >> 17)) & 0x01;
        osc->ShiftRegister = ((osc->ShiftRegister << 1) & 0x7fffff) | osc->Bit0;
    }

	OSC* osc_source = osc->OscSource;
	OSC* osc_destination = osc->OscDestination;

    /// Oscilltor mit Source Sycronisieren ?? ///
#ifndef DISABLE_SYNC
    if (osc->FrequenzCounterMsbRising && osc_destination->SyncBit && !(osc->SyncBit && osc_source->FrequenzCounterMsbRising))
        osc_destination->FrequenzCounter = 0;
#endif
}

inline void OscExecuteCycles(OSC* osc, uint8_t cycles)
{
	///////////////////// VOICE 1 ////////////////////

    if(!osc->TestBit)
    {
        osc->FrequenzCounterOld = osc->FrequenzCounter;
        osc->FrequenzCounter += osc->FrequenzAdd * cycles;
        osc->FrequenzCounter &= 0xffffff;
    }

    osc->FrequenzCounterMsbRising = !(osc->FrequenzCounterOld & 0x800000) && (osc->FrequenzCounter & 0x800000);

    if(!(osc->FrequenzCounterOld & 0x080000) && (osc->FrequenzCounter & 0x080000))
    {
        osc->Bit0 = ((osc->ShiftRegister >> 22) ^ (osc->ShiftRegister >> 17)) & 0x01;
        osc->ShiftRegister = ((osc->ShiftRegister << 1) & 0x7fffff) | osc->Bit0;
    }

	OSC* osc_source = osc->OscSource;
	OSC* osc_destination = osc->OscDestination;

    /// Oscilltor mit Source Sycronisieren ?? ///
#ifndef DISABLE_SYNC
    if (osc->FrequenzCounterMsbRising && osc_destination->SyncBit && !(osc->SyncBit && osc_source->FrequenzCounterMsbRising))
        osc_destination->FrequenzCounter = 0;
#endif

	///////////////////// VOICE 2 ////////////////////

	osc++;
    if(!osc->TestBit)
    {
        osc->FrequenzCounterOld = osc->FrequenzCounter;
        osc->FrequenzCounter += osc->FrequenzAdd * cycles;
        osc->FrequenzCounter &= 0xffffff;
    }

    osc->FrequenzCounterMsbRising = !(osc->FrequenzCounterOld & 0x800000) && (osc->FrequenzCounter & 0x800000);

    if(!(osc->FrequenzCounterOld & 0x080000) && (osc->FrequenzCounter & 0x080000))
    {
        osc->Bit0 = ((osc->ShiftRegister >> 22) ^ (osc->ShiftRegister >> 17)) & 0x01;
        osc->ShiftRegister = ((osc->ShiftRegister << 1) & 0x7fffff) | osc->Bit0;
    }

	osc_source = osc->OscSource;
	osc_destination = osc->OscDestination;

    /// Oscilltor mit Source Sycronisieren ?? ///
#ifndef DISABLE_SYNC
    if (osc->FrequenzCounterMsbRising && osc_destination->SyncBit && !(osc->SyncBit && osc_source->FrequenzCounterMsbRising))
        osc_destination->FrequenzCounter = 0;
#endif

	///////////////////// VOICE 3 ////////////////////
	
	osc++;
    if(!osc->TestBit)
    {
        osc->FrequenzCounterOld = osc->FrequenzCounter;
        osc->FrequenzCounter += osc->FrequenzAdd * cycles;
        osc->FrequenzCounter &= 0xffffff;
    }

    osc->FrequenzCounterMsbRising = !(osc->FrequenzCounterOld & 0x800000) && (osc->FrequenzCounter & 0x800000);

    if(!(osc->FrequenzCounterOld & 0x080000) && (osc->FrequenzCounter & 0x080000))
    {
        osc->Bit0 = ((osc->ShiftRegister >> 22) ^ (osc->ShiftRegister >> 17)) & 0x01;
        osc->ShiftRegister = ((osc->ShiftRegister << 1) & 0x7fffff) | osc->Bit0;
    }

	osc_source = osc->OscSource;
	osc_destination = osc->OscDestination;

    /// Oscilltor mit Source Sycronisieren ?? ///
#ifndef DISABLE_SYNC
    if (osc->FrequenzCounterMsbRising && osc_destination->SyncBit && !(osc->SyncBit && osc_source->FrequenzCounterMsbRising))
        osc_destination->FrequenzCounter = 0;
#endif
}