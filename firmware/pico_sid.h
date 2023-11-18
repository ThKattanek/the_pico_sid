//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: pico_sid.h                            //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
////////////////////////////////////////////////// 

#ifndef PICO_SID_CLASS_H
#define PICO_SID_CLASS_H

enum sid_type {MOS_6581, MOS_8580};

#include <stdint.h>
class PICO_SID
{
	public:
	PICO_SID();
	~PICO_SID();

	void SetSidType(sid_type type);
	void NextCycles(int cycle_count);
	void Reset();
	void WriteReg(uint8_t address, uint8_t value);
	uint8_t ReadReg(uint8_t address);
	uint16_t AudioOut();
};

#endif // PICO_SID_CLASS_H
