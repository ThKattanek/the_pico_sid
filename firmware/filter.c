//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: filter.c                              //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
//////////////////////////////////////////////////

#include "filter.h"
#include "f0_points_6581.h"
#include "f0_points_8580.h"

inline void FilterSetSidModel(FILTER* filter, int sid_model)
{
	filter->SidModel = sid_model;

	switch (filter->SidModel)
	{
	case MOS_6581:
        filter->WaveZero = 0x380;
        filter->VoiceDC = 0x800 * 0xFF;

        filter->MixerDC = -0xfff * 0xff / 18 >> 7;
        filter->f0 = f0_6581;
		break;

	case MOS_8580:
        filter->WaveZero = 0x800;
        filter->VoiceDC = 0;

        filter->MixerDC = 0;
        filter->f0 = f0_8580;
		break;	
	
	default:
		break;
	}
}

inline void FilterInit(FILTER* filter)
{
	filter->FilterOn = true;
	FilterReset(filter);
}

inline void FilterReset(FILTER* filter)
{
    filter->FilterFrequenz = 0;
    filter->FilterResonanz = 0;
    filter->FilterKey = 0;
    filter->Voice3Off = 0;
    filter->HpBpLp = 0;
    filter->Volume = 0;

    filter->Vhp = 0;
    filter->Vbp = 0;
    filter->Vlp = 0;
    filter->Vnf = 0;

    FilterSetW0(filter);
    FilterSetQ(filter);
}

inline void FilterExcecuteCycles(FILTER* filter, int cycles, int voice1, int voice2, int voice3, int ext_in)
{
   	voice1 >>= 0;
    voice2 >>= 0;

    if(filter->Voice3Off && !(filter->FilterKey & 0x04)) voice3 = 0;
    else voice3 >>= 0;

    ext_in >>= 0;

    if(!filter->FilterOn)
    {
        filter->Vnf = voice1 + voice2 + voice3 + ext_in;
        filter->Vhp = filter->Vbp = filter->Vlp = 0;
        return;
    }

    int Vi;

    switch (filter->FilterKey)
    {

    default:

    case 0x00:
        Vi = 0;
        filter->Vnf = voice1 + voice2 + voice3 + ext_in;
        break;

    case 0x01:
        Vi = voice1;
        filter->Vnf = voice2 + voice3 + ext_in;
        break;

    case 0x02:
        Vi = voice2;
        filter->Vnf = voice1 + voice3 + ext_in;
        break;

    case 0x03:
        Vi = voice1 + voice2;
        filter->Vnf = voice3 + ext_in;
        break;

    case 0x04:
        Vi = voice3;
        filter->Vnf = voice1 + voice2 + ext_in;
        break;

    case 0x05:
        Vi = voice1 + voice3;
        filter->Vnf = voice2 + ext_in;
        break;

    case 0x06:
        Vi = voice2 + voice3;
        filter->Vnf = voice1 + ext_in;
        break;

    case 0x07:
        Vi = voice1 + voice2 + voice3;
        filter->Vnf = ext_in;
        break;

    case 0x08:
        Vi = ext_in;
        filter->Vnf = voice1 + voice2 + voice3;
        break;

    case 0x09:
        Vi = voice1 + ext_in;
        filter->Vnf = voice2 + voice3;
        break;

    case 0x0A:
        Vi = voice2 + ext_in;
        filter->Vnf = voice1 + voice3;
        break;

    case 0x0B:
        Vi = voice1 + voice2 + ext_in;
        filter->Vnf = voice3;
        break;

    case 0x0C:
        Vi = voice3 + ext_in;
        filter->Vnf = voice1 + voice2;
        break;

    case 0x0D:
        Vi = voice1 + voice3 + ext_in;
        filter->Vnf = voice2;
        break;

    case 0x0E:
        Vi = voice2 + voice3 + ext_in;
        filter->Vnf = voice1;
        break;

    case 0x0F:
        Vi = voice1 + voice2 + voice3 + ext_in;
        filter->Vnf = 0;
        break;
    }

    int delta_t_flt = 8;
    while (cycles)
    {
        if (cycles < delta_t_flt)
        {
            delta_t_flt = cycles;
        }
        int w0_delta_t = filter->w0_ceil_dt * delta_t_flt >> 6;
        int dVbp = (w0_delta_t * filter->Vhp >> 14);
        int dVlp = (w0_delta_t * filter->Vbp >> 14);
        filter->Vbp -= dVbp;
        filter->Vlp -= dVlp;
        filter->Vhp = (filter->Vbp * filter->_1024_div_Q >> 10) - filter->Vlp - Vi;
        cycles -= delta_t_flt;
    }
}

inline void FilterEnableFilter(FILTER* filter, bool enabled)
{
	filter->FilterOn = enabled;
}

inline int  FilterGetOutput(FILTER* filter)
{
    if (!filter->FilterOn)
    {
        return (filter->Vnf + filter->MixerDC) * (int)(filter->Volume);
    }

    int Vf;

    switch(filter->HpBpLp)
    {
    default:

    case 0x00:
        Vf = 0;
        break;

    case 0x01:
        Vf = filter->Vlp;
        break;

    case 0x02:
        Vf = filter->Vbp;
        break;

    case 0x03:
        Vf = filter->Vlp + filter->Vbp;
        break;

    case 0x04:
        Vf = filter->Vhp;
        break;

    case 0x05:
        Vf = filter->Vlp + filter->Vhp;
        break;

    case 0x06:
        Vf = filter->Vbp + filter->Vhp;
        break;

    case 0x07:
        Vf = filter->Vlp + filter->Vbp + filter->Vhp;
        break;
    }

    return (filter->Vnf + Vf + filter->MixerDC) * (int)(filter->Volume);
}

inline void FilterSetFrequenz(FILTER* filter, uint16_t filter_frequenz)
{
	filter->FilterFrequenz = filter_frequenz & 0x07FF;
	FilterSetW0(filter);
}

inline void FilterSetControl1(FILTER* filter, uint8_t value)
{
	filter->FilterResonanz = (value >> 4);
	filter->FilterKey = value & 0x0f;
	FilterSetQ(filter);
}

inline void FilterSetControl2(FILTER* filter, uint8_t value)
{
	filter->Voice3Off = value & 0x80;
	filter->HpBpLp = (value >> 4) & 0x07;
	filter->Volume = value & 0x0F;
}

inline void FilterSetW0(FILTER* filter)
{
    const float pi = 3.1415926535897932385;
    filter->w0 = (int)(2 * pi * filter->f0[filter->FilterFrequenz] * 1.048576);
    const int w0_max_1 = (int)(2*pi*16000*1.048576);
    filter->w0_ceil_1 = filter->w0 <= w0_max_1 ? filter->w0 : w0_max_1;
    const int w0_max_dt = (int)(2*pi*4000*1.048576);
    filter->w0_ceil_dt = filter->w0 <= w0_max_dt ? filter->w0 : w0_max_dt;
}

inline void FilterSetQ(FILTER* filter)
{
	filter->_1024_div_Q = (int)(1024.0/(0.707 + 1.0 * filter->FilterResonanz/0x0F));
}