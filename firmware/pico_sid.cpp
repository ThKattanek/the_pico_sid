//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: pico_sid.cc                           //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
//////////////////////////////////////////////////

#include "pico_sid.h"

PICO_SID::PICO_SID()
{
}

PICO_SID::~PICO_SID()
{
}

uint8_t PICO_SID::ReadReg(uint8_t address)
{
	return 0;
}

uint16_t PICO_SID::AudioOut()
{
	return 0;
}

void PICO_SID::SetSidType(sid_type type)
{
}

void PICO_SID::NextCycles(int cycle_count)
{
	 int i;
}

void PICO_SID::Reset()
{
}

void PICO_SID::WriteReg(uint8_t address, uint8_t value)
{
}
