//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid_extfilter.cpp                     //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
// The template used was resid:                 //
// https://github.com/libsidplayfp/resid        //
//                                              //
//////////////////////////////////////////////////

#include "./sid_extfilter.h"

SID_EXTFILTER::SID_EXTFILTER()
{
	Reset();
	EnableFilter(true);
	SetSidType(MOS_6581);

	w0lp = 104858;
	w0hp = 105;
}

void SID_EXTFILTER::SetSidType(sid_type type)
{
	if(type == MOS_6581)
		mixer_dc = ((((0x800 - 0x380) + 0x800)*0xff*3 - 0xfff*0xff/18) >> 7)*0x0f;
	else
		mixer_dc = 0;
}

void SID_EXTFILTER::EnableFilter(bool enable)
{
	enabled = enable;
}

void SID_EXTFILTER::Reset()
{
	Vlp = 0;
	Vhp = 0;
	Vo = 0;
}
