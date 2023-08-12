//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid.h                                 //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
//////////////////////////////////////////////////

#ifndef SIDCLASS_H
#define SIDCLASS_H

#include <pico/stdlib.h>

#define SID_CYCLE_EXACT
struct OSC
{
    uint32_t	FrequenzCounter;
    uint32_t	FrequenzCounterOld;
    bool		FrequenzCounterMsbRising;   /// (True wenn MSB von 0 nach 1 wechselt ansonsten False)
    uint32_t	ShiftRegister;
    uint32_t	Bit0;
    uint16_t	FrequenzAdd;
    uint16_t	PulseCompare;
    uint8_t		WaveForm;
    bool		TestBit;
    bool		RingBit;
    bool		SyncBit;
}typedef OSC;
struct SID
{
	// Alle IO Register ///
	uint8_t   IO[32];
	uint8_t   Freq0Lo;
	uint8_t   Freq0Hi;
	uint8_t   Freq1Lo;
	uint8_t   Freq1Hi;
	uint8_t   Freq2Lo;
	uint8_t   Freq2Hi;
	uint8_t   Puls0Lo;
	uint8_t   Puls0Hi;
	uint8_t   Puls1Lo;
	uint8_t   Puls1Hi;
	uint8_t   Puls2Lo;
	uint8_t   Puls2Hi;
	uint8_t   Ctrl0;
	uint8_t   Ctrl1;
	uint8_t   Ctrl2;
	uint8_t   FilterFreqLo;
	uint8_t   FilterFreqHi;

OSC	Osc[3];
}typedef SID;

void SidReset(SID *sid);
void SidWriteReg(SID *sid, uint8_t address, uint8_t value);

void OscReset(OSC *osc);
void OscSetFrequenz(OSC *osc, uint16_t frequency);
void OscSetControlBits(OSC *osc, uint8_t ctrlbits);
void OscSetPulesCompare(OSC *osc, uint16_t pulsecompare);
uint16_t OscGetDreieck(OSC *osc);
uint16_t OscGetOutput(OSC *osc);
void OscOneCycle(OSC *osc);
void OscExecuteCycles(OSC *osc, uint8_t cycles);

#endif // SID_H