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
    SetSidType(MOS_6581);

    voice[0].SetSyncSource(&voice[2]);
    voice[1].SetSyncSource(&voice[0]);
    voice[2].SetSyncSource(&voice[1]);
}

PICO_SID::~PICO_SID()
{
}

uint8_t PICO_SID::ReadReg(uint8_t)
{
    return 0;
}

int PICO_SID::AudioOut()
{
    // Ausgabe der reinen Wellenform
    // return voice[0].wave.Output() + voice[1].wave.Output() + voice[2].wave.Output();
    // Ausgabe Amplituden moduliert
    return ((voice[0].Output() + voice[1].Output() + voice[2].Output()) / (float)0x2ffffd) * 0xffff;
}

void PICO_SID::SetSidType(sid_type type)
{
    sid_model = type;

    for (int i = 0; i < 3; i++)
        voice[i].SetSidType(type);
}

void PICO_SID::Clock(cycle_count delta_t)
{
    int i;

    if (unlikely(delta_t <= 0))
    {
        return;
    }

    for (i = 0; i < 3; i++)
    {
        voice[i].envelope.Clock(delta_t);
    }

    cycle_count delta_t_osc = delta_t;
    while (delta_t_osc) {
        cycle_count delta_t_min = delta_t_osc;

        for (i = 0; i < 3; i++) {
            SID_WAVE& wave = voice[i].wave;

            if (likely(!(wave.sync_dest->sync && wave.freq))) {
                continue;
            }

            reg16 freq = wave.freq;
            reg24 accumulator = wave.accumulator;
            reg24 delta_accumulator =
                (accumulator & 0x800000 ? 0x1000000 : 0x800000) - accumulator;

            cycle_count delta_t_next = delta_accumulator/freq;
            if (likely(delta_accumulator%freq)) {
                ++delta_t_next;
            }

            if (unlikely(delta_t_next < delta_t_min)) {
                delta_t_min = delta_t_next;
            }
        }

        for (i = 0; i < 3; i++) {
            voice[i].wave.Clock(delta_t_min);
        }

        for (i = 0; i < 3; i++) {
            voice[i].wave.Synchronize();
        }

        delta_t_osc -= delta_t_min;
    }

    for (i = 0; i < 3; i++)
    {
        voice[i].wave.SetWaveformOutput(delta_t);
    }

    // Clock filter.
    // filter.clock(delta_t, voice[0].output(), voice[1].output(), voice[2].output());

    // Clock external filter.
    // extfilt.clock(delta_t, filter.output());
}

void PICO_SID::Reset()
{
    for(int i=0; i<3; i++)
        voice[i].Reset();

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
         //filter.writeFC_LO(databus_value);
         break;
     case 0x16:
         //filter.writeFC_HI(databus_value);
         break;
     case 0x17:
         //filter.writeRES_FILT(databus_value);
         break;
     case 0x18:
         //filter.writeMODE_VOL(databus_value);
         break;
     default:
         break;
     }
}
