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

PICO_SID::PICO_SID()
{
    voice[0].SetSyncSource(&voice[2]);
    voice[1].SetSyncSource(&voice[0]);
    voice[2].SetSyncSource(&voice[1]);

    Reset();
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
    // Ausgabe der reinen Wellenform
    return voice[0].wave.Output() + voice[1].wave.Output() + voice[2].wave.Output();
}

void PICO_SID::SetSidType(sid_type type)
{
}

void PICO_SID::Clock()
{
    int i;

    // Clock oscillators.
    for (i = 0; i < 3; i++) {
        voice[i].wave.Clock();
    }

    // Synchronize oscillators.
    for (i = 0; i < 3; i++) {
        voice[i].wave.Synchronize();
    }

    // Calculate waveform output.
    for (i = 0; i < 3; i++) {
        voice[i].wave.SetWaveformOutput();
    }
}

void PICO_SID::Clock(cycle_count delta_t)
{

}

void PICO_SID::Reset()
{
     voice[0].Reset();
     voice[1].Reset();
     voice[2].Reset();
}

void PICO_SID::WriteReg(uint8_t address, uint8_t value)
{
     switch (address & 0x1f) {
     case 0x00:
         voice[0].wave.WriteFreqLo(value);
         break;
     case 0x01:
         voice[0].wave.WriteFreqHi(value);
         break;
     case 0x02:
         voice[0].wave.WritePwLo(value);
         break;
     case 0x03:
         voice[0].wave.WritePwHi(value);
         break;
     case 0x04:
         voice[0].WriteControlReg(value);
         break;
     case 0x05:
         //voice[0].envelope.writeATTACK_DECAY(bus_value);
         break;
     case 0x06:
         //voice[0].envelope.writeSUSTAIN_RELEASE(bus_value);
         break;
     case 0x07:
         voice[1].wave.WriteFreqLo(value);
         break;
     case 0x08:
         voice[1].wave.WriteFreqHi(value);
         break;
     case 0x09:
         voice[1].wave.WritePwLo(value);
         break;
     case 0x0a:
         voice[1].wave.WritePwHi(value);
         break;
     case 0x0b:
         voice[1].WriteControlReg(value);
         break;
     case 0x0c:
         //voice[1].envelope.writeATTACK_DECAY(bus_value);
         break;
     case 0x0d:
         //voice[1].envelope.writeSUSTAIN_RELEASE(bus_value);
         break;
     case 0x0e:
         voice[2].wave.WriteFreqLo(value);
         break;
     case 0x0f:
         voice[2].wave.WriteFreqHi(value);
         break;
     case 0x10:
         voice[2].wave.WritePwLo(value);
         break;
     case 0x11:
         voice[2].wave.WritePwHi(value);
         break;
     case 0x12:
         voice[2].WriteControlReg(value);
         break;
     case 0x13:
         //voice[2].envelope.writeATTACK_DECAY(bus_value);
         break;
     case 0x14:
         //voice[2].envelope.writeSUSTAIN_RELEASE(bus_value);
         break;
     case 0x15:
         //filter.writeFC_LO(bus_value);
         break;
     case 0x16:
         //filter.writeFC_HI(bus_value);
         break;
     case 0x17:
         //filter.writeRES_FILT(bus_value);
         break;
     case 0x18:
         //filter.writeMODE_VOL(bus_value);
         break;
     default:
         break;
     }
}
