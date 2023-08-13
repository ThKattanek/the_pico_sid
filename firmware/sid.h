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

#ifndef SID_H
#define SID_H

#include <pico/stdlib.h>

#include "osc.h"
#include "env.h"

//#define SID_CYCLE_EXACT

struct SID
{
	// Alle IO Register ///
	uint8_t   	IO[32];
	uint8_t   	Freq0Lo;
	uint8_t   	Freq0Hi;
	uint8_t   	Freq1Lo;
	uint8_t   	Freq1Hi;
	uint8_t   	Freq2Lo;
	uint8_t   	Freq2Hi;
	uint8_t   	Puls0Lo;
	uint8_t   	Puls0Hi;
	uint8_t   	Puls1Lo;
	uint8_t   	Puls1Hi;
	uint8_t   	Puls2Lo;
	uint8_t   	Puls2Hi;
	uint8_t   	Ctrl0;
	uint8_t   	Ctrl1;
	uint8_t   	Ctrl2;
	uint8_t  	FilterFreqLo;
	uint8_t   	FilterFreqHi;

	OSC		  	Osc[3];
	ENV			Env[3];
	float		VolumeOut;
}typedef SID;

void SidInit(SID *sid);
void SidReset(SID *sid);
void SidWriteReg(SID *sid, uint8_t address, uint8_t value);

#endif // SID_H