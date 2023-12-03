//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: pico_sid.cc                           //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
// The template used was resid:                 //
// https://github.com/libsidplayfp/resid        //
//                                              //
//////////////////////////////////////////////////

#include "./pico_sid.h"
#include "pico_sid.h"

PICO_SID::PICO_SID()
{
    voice[0].SetSyncSource(&voice[2]);
    voice[1].SetSyncSource(&voice[0]);
    voice[2].SetSyncSource(&voice[1]);

	Reset();

    ext_in = 0;
	filter_enable = true;
	extfilter_enable = true;
	digi_boost_enable = false;

	digi_level[0][0] = 18000;	// MOS6581 Digiboos off
	digi_level[0][1] = 18000;	// MOS6581 Digiboos on
	digi_level[1][0] = 1000;	// MOS8580 Digiboos off
	digi_level[1][1] = 20000;	// MOS8580 Digiboos on

	SetSidType(MOS_6581);
}

PICO_SID::~PICO_SID()
{
}

uint8_t PICO_SID::ReadReg(uint8_t)
{
    return 0;
}

void PICO_SID::Input(int sample)
{
    ext_in = (sample << 4)*3;
}

void PICO_SID::SetSidType(sid_type type)
{
    sid_model = type;

    for (int i = 0; i < 3; i++)
        voice[i].SetSidType(type);

    filter.SetSidType(type);
	extfilter.SetSidType(type);

	SetExtIn();
}

void PICO_SID::EnableFilter(bool enable)
{
	filter_enable = enable;
	filter.EnableFilter(filter_enable);
}

void PICO_SID::EnableExtFilter(bool enable)
{
	extfilter_enable = enable;
	extfilter.EnableFilter(extfilter_enable);
}

void PICO_SID::EnableDigiBoost8580(bool enable)
{
	digi_boost_enable = enable;
	SetExtIn();
}

void PICO_SID::Reset()
{
    for(int i=0; i<3; i++)
	{
        voice[i].Reset();
	}

    filter.Reset();
	extfilter.Reset();
}

void PICO_SID::WriteReg(uint8_t write_address, uint8_t bus_value)
{
     switch (write_address & 0x1f) {
     case 0x00:
         voice[0].wave.WriteFreqLo(bus_value);
         break;
     case 0x01:
         voice[0].wave.WriteFreqHi(bus_value);
         break;
     case 0x02:
         voice[0].wave.WritePwLo(bus_value);
         break;
     case 0x03:
         voice[0].wave.WritePwHi(bus_value);
         break;
     case 0x04:
         voice[0].WriteControlReg(bus_value);
         break;
     case 0x05:
         voice[0].envelope.WriteAttackDecay(bus_value);
         break;
     case 0x06:
         voice[0].envelope.WriteSustainRelease(bus_value);
         break;
     case 0x07:
         voice[1].wave.WriteFreqLo(bus_value);
         break;
     case 0x08:
         voice[1].wave.WriteFreqHi(bus_value);
         break;
     case 0x09:
         voice[1].wave.WritePwLo(bus_value);
         break;
     case 0x0a:
         voice[1].wave.WritePwHi(bus_value);
         break;
     case 0x0b:
         voice[1].WriteControlReg(bus_value);
         break;
     case 0x0c:
         voice[1].envelope.WriteAttackDecay(bus_value);
         break;
     case 0x0d:
         voice[1].envelope.WriteSustainRelease(bus_value);
         break;
     case 0x0e:
         voice[2].wave.WriteFreqLo(bus_value);
         break;
     case 0x0f:
         voice[2].wave.WriteFreqHi(bus_value);
         break;
     case 0x10:
         voice[2].wave.WritePwLo(bus_value);
         break;
     case 0x11:
         voice[2].wave.WritePwHi(bus_value);
         break;
     case 0x12:
         voice[2].WriteControlReg(bus_value);
         break;
     case 0x13:
         voice[2].envelope.WriteAttackDecay(bus_value);
         break;
     case 0x14:
         voice[2].envelope.WriteSustainRelease(bus_value);
         break;
     case 0x15:
         filter.WriteFcLo(bus_value);
         break;
     case 0x16:
         filter.WriteFcHi(bus_value);
         break;
     case 0x17:
         filter.WriteResFilt(bus_value);
         break;
     case 0x18:
         filter.WriteModeVol(bus_value);
         break;
     default:
         break;
     }
}
