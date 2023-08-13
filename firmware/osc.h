//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: osc.h                                 //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
//////////////////////////////////////////////////

#ifndef OSC_H
#define OSC_H

#include <pico/stdlib.h>

#define DISABLE_RINGMOD
#define DISABLE_SYNC

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
	void*		OscSource;
	void*		OscDestination;
}typedef OSC;

void OscReset(OSC *osc);
void OscSetFrequenz(OSC* osc, uint16_t frequency);
void OscSetControlBits(OSC* osc, uint8_t ctrlbits);
void OscSetPulesCompare(OSC* osc, uint16_t pulsecompare);
uint16_t OscGetDreieck(OSC* osc);
uint16_t OscGetOutput(OSC* osc);
void OscOneCycle(OSC* osc);
void OscExecuteCycles(OSC* osc, uint8_t cycles);

#endif // OSC_H