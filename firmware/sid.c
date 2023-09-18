//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid.c                                 //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
//////////////////////////////////////////////////

#include "sid.h"

unsigned int RateCounterPeriod[]={9,32,63,95,149,220,267,313,392,977,1954,3126,3907,11720,19532,31251};
unsigned int SustainLevel[]={0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};

#include "mos6581_8085_waves.h"

////////// SID //////////

uint		sid_type;
uint		voice_dc;
uint		wave_zero;
bool		enable_audio_out;
uint8_t		last_reg_write_value;

////////// VOICES //////////

VOICE 		voices[3];
uint		*wave0;
uint		*wave1;
uint		*wave2;
uint		*wave3;

////////// FILTER //////////

bool		filter_on;
uint		filter_key;
uint		filter_frequency;
uint		filter_resonance;
uint		voice3_off;
uint		hp_bp_lp;
uint		volume;

/// FILTER internal Regs ///
volatile int	mixer_dc;
volatile int	vhp;
volatile int	vbp;
volatile int	vlp;
volatile int	vnf;
volatile int	w0,w0_ceil_1,w0_ceil_dt;
volatile int	_1024_div_Q;
volatile int*   f0;

volatile int	ext_in;

void SidInit()
{
	voices[0].osc_enable = true;
	voices[1].osc_enable = true;
	voices[2].osc_enable = true;

	voices[0].osc_source = 2;
	voices[1].osc_source = 0;
	voices[2].osc_source = 1;

	voices[0].osc_destination = 1;
	voices[1].osc_destination = 2;
	voices[2].osc_destination = 0;

    filter_on = true;

	enable_audio_out = true;

    SidSetChipTyp(MOS_8580);
	SidReset();
}

void SidSetChipTyp(int chip_type)
{
	sid_type = chip_type;

    if(sid_type == MOS_6581)
    {
        wave0 = Wave6581_0;
        wave1 = Wave6581_1;
        wave2 = Wave6581_2;
        wave3 = Wave6581_3;

        wave_zero = 0x380;			// 0x380
        voice_dc = 0x800 * 0xFF;	// 0x800 * 0xFF

        mixer_dc = -0xfff * 0xff / 18 >> 7;
        f0 = f0_6581;
    }
    else
    {
        wave0 = Wave8580_0;
        wave1 = Wave8580_1;
        wave2 = Wave8580_2;
        wave3 = Wave8580_3;

        wave_zero = 0x800;	// 0x800
        voice_dc = 0x000;	// 0x000

        mixer_dc = 0;
        f0 = f0_8580;
    }

    SidSetW0();
    SidSetQ();
}

void SidEnableFilter(bool enable)
{
	filter_on = enable;
}

inline void SidSetAudioOut(bool enable)
{
	enable_audio_out = enable;
}

inline void SidReset()
{
	for(int i=0; i<3; i++)
	{
		/// OSC Reset
		voices[i].accu = 0;
		voices[i].shiftreg = 0x7FFFF8;
		voices[i].frequency = 0;
		voices[i].dutycycle = 0;
		voices[i].test_bit = false;
		voices[i].ring_bit = false;
		voices[i].sync_bit = false;
		voices[i].msb_rising = false;

		/// ENV Reset
		voices[i].env_counter = 0;
		voices[i].attack = 0;
		voices[i].decay = 0;
		voices[i].sustain = 0;
		voices[i].release = 0;
		voices[i].key_bit = false;
		voices[i].rate_counter = 0;
		voices[i].state = RELEASE;
		voices[i].rate_period = RateCounterPeriod[voices[i].release];
		voices[i].hold_zero = true;
		voices[i].exponential_counter = 0;
		voices[i].exponential_counter_period = 1;
	}

	/// FILTER Reset
	filter_frequency = 0;
    filter_resonance = 0;
    filter_key = 0;
    voice3_off = 0;
    hp_bp_lp = 0;
    volume = 0;

    vhp=0;
    vbp=0;
    vlp=0;
    vnf=0;

    SidSetW0();
    SidSetQ();
}

inline void SidWriteReg(uint8_t address, uint8_t value)
{
    static bool key_next;

	last_reg_write_value = value;

    switch(address & 0x1f)
    {
    case 0: // FrequenzLO für Stimme 0
        voices[0].frequency = (voices[0].frequency & 0xff00) | (value & 0x00ff);
        break;

    case 1: // FrequenzHI für Stimme 0
        voices[0].frequency = ((value<<8) & 0xff00) | (voices[0].frequency & 0x00ff);
        break;

    case 2: // PulsweiteLO für Stimme 0
        voices[0].dutycycle = (voices[0].dutycycle & 0xf00) | (value & 0x0ff);
        break;

    case 3: // PulsweiteHI für Stimme 0
        voices[0].dutycycle = ((value<<8) & 0xf00) | (voices[0].dutycycle & 0x0ff);
        break;

    case 4: // Kontrol Register für Stimme 0
        voices[0].waveform = (value>>4) & 0x0f;
        voices[0].ring_bit = !(~value & 0x04);
        voices[0].sync_bit = !(~value & 0x02);
        if(value & 0x08)
        {
            voices[0].accu = 0;
            voices[0].shiftreg = 0;
        }
        else if(voices[0].test_bit) voices[0].shiftreg = 0x7ffff8;
        voices[0].test_bit = !(~value & 0x08);
        
        key_next = value & 0x01;
        if (!voices[0].key_bit && key_next)
        {
            voices[0].state = ATTACK;
            voices[0].rate_period = RateCounterPeriod[voices[0].attack];
            voices[0].hold_zero = false;
        }
        else if (voices[0].key_bit && !key_next)
        {
            voices[0].state = RELEASE;
            voices[0].rate_period = RateCounterPeriod[voices[0].release];
        }
        voices[0].key_bit = key_next;
        break;

    case 5: // Attack-Decay für Stimme 0
        voices[0].attack = (value>>4) & 0x0f;
        voices[0].decay = value & 0x0f;
        if(voices[0].state == ATTACK) voices[0].rate_period = RateCounterPeriod[voices[0].attack];
        else if (voices[0].state == DECAY_SUSTAIN) voices[0].rate_period = RateCounterPeriod[voices[0].decay];
        break;

    case 6:		// Sustain-Release für Stimme 0
        voices[0].sustain = (value>>4) & 0x0f;
        voices[0].release = value & 0x0f;
        if (voices[0].state == RELEASE) voices[0].rate_period = RateCounterPeriod[voices[0].release];
        break;
        
        /////////////////////////////////////////////////////////////

    case 7: // FrequenzLO für Stimme 1
        voices[1].frequency = (voices[1].frequency & 0xff00) | (value & 0x00ff);
        break;

    case 8: // FrequenzHI für Stimme 1
        voices[1].frequency = ((value<<8) & 0xff00) | (voices[1].frequency & 0x00ff);
        break;

    case 9: // PulsweiteLO für Stimme 1
        voices[1].dutycycle = (voices[1].dutycycle & 0xf00) | (value & 0x0ff);
        break;

    case 10: // PulsweiteHI für Stimme 1
        voices[1].dutycycle = ((value<<8) & 0xf00) | (voices[1].dutycycle & 0x0ff);
        break;

    case 11: // Kontrol Register für Stimme 1
        voices[1].waveform = (value>>4) & 0x0f;
        voices[1].ring_bit = !(~value & 0x04);
        voices[1].sync_bit = !(~value & 0x02);
        if(value & 0x08)
        {
            voices[1].accu = 0;
            voices[1].shiftreg = 0;
        }
        else if(voices[1].test_bit) voices[1].shiftreg = 0x7ffff8;
        voices[1].test_bit = !(~value & 0x08);
        
        key_next = value & 0x01;
        if (!voices[1].key_bit && key_next)
        {
            voices[1].state = ATTACK;
            voices[1].rate_period = RateCounterPeriod[voices[1].attack];
            voices[1].hold_zero = false;
        }
        else if (voices[1].key_bit && !key_next)
        {
            voices[1].state = RELEASE;
            voices[1].rate_period = RateCounterPeriod[voices[1].release];
        }
        voices[1].key_bit = key_next;
        break;

    case 12: // Attack-Decay für Stimme 1
        voices[1].attack = (value>>4) & 0x0f;
        voices[1].decay = value & 0x0f;
        if(voices[1].state == ATTACK) voices[1].rate_period = RateCounterPeriod[voices[1].attack];
        else if (voices[1].state == DECAY_SUSTAIN) voices[1].rate_period = RateCounterPeriod[voices[1].decay];
        break;

    case 13:		// Sustain-Release für Stimme 1
        voices[1].sustain = (value>>4) & 0x0f;
        voices[1].release = value & 0x0f;
        if (voices[1].state == RELEASE) voices[1].rate_period = RateCounterPeriod[voices[1].release];
        break;
        
        /////////////////////////////////////////////////////////////

    case 14: // FrequenzLO für Stimme 2
        voices[2].frequency = (voices[2].frequency & 0xff00) | (value & 0x00ff);
        break;

    case 15: // FrequenzHI für Stimme 2
        voices[2].frequency = ((value<<8) & 0xff00) | (voices[2].frequency & 0x00ff);
        break;

    case 16: // PulsweiteLO für Stimme 02
        voices[2].dutycycle = (voices[2].dutycycle & 0xf00) | (value & 0x0ff);
        break;

    case 17: // PulsweiteHI für Stimme 2
        voices[2].dutycycle = ((value<<8) & 0xf00) | (voices[2].dutycycle & 0x0ff);
        break;

    case 18: // Kontrol Register für Stimme 2
        voices[2].waveform = (value>>4) & 0x0f;
        voices[2].ring_bit = !(~value & 0x04);
        voices[2].sync_bit = !(~value & 0x02);
        if(value & 0x08)
        {
            voices[2].accu = 0;
            voices[2].shiftreg = 0;
        }
        else if(voices[2].test_bit) voices[2].shiftreg = 0x7ffff8;
        voices[2].test_bit = !(~value & 0x08);
        
        key_next = value & 0x01;
        if (!voices[2].key_bit && key_next)
        {
            voices[2].state = ATTACK;
            voices[2].rate_period = RateCounterPeriod[voices[2].attack];
            voices[2].hold_zero = false;
        }
        else if (voices[2].key_bit && !key_next)
        {
            voices[2].state = RELEASE;
            voices[2].rate_period = RateCounterPeriod[voices[2].release];
        }
        voices[2].key_bit = key_next;
        break;

    case 19: // Attack-Decay für Stimme 2
        voices[2].attack = (value>>4) & 0x0f;
        voices[2].decay = value & 0x0f;
        if(voices[2].state == ATTACK) voices[2].rate_period = RateCounterPeriod[voices[2].attack];
        else if (voices[2].state == DECAY_SUSTAIN) voices[2].rate_period = RateCounterPeriod[voices[2].decay];
        break;

    case 20:  // Sustain-Release für Stimme 2
        voices[2].sustain = (value>>4) & 0x0f;
        voices[2].release = value & 0x0f;
        if (voices[2].state == RELEASE) voices[2].rate_period = RateCounterPeriod[voices[2].release];
        break;

	/////////////////////////////////////////////////////////////

    case 21: // FilterfrequenzLO
        filter_frequency = (filter_frequency & 0x7f8) | (value & 0x007);
        SidSetW0();
        break;

    case 22: // FilterfrequenzHI
        filter_frequency = ((value<<3) & 0x7f8) | (filter_frequency & 0x007);
        SidSetW0();
        break;

    case 23: // Filterkontrol_1 und Resonanz
        filter_resonance = (value>>4) & 0x0f;
        SidSetQ();
        filter_key = value & 0x0f;
        break;

    case 24: // Filterkontrol_2 und Lautstärke
        voice3_off = value & 0x80;
        hp_bp_lp = (value >> 4) & 0x07;
        volume = value & 0x0f;
        break;
    }
}

inline uint8_t SidReadReg(uint8_t address)
{
    switch(address & 0x1f)
    {
	case 25: // AD Wandler 1 (POTX)
		return 0x88;
		break;

    case 26: // AD Wandler 2 (POTY)
		return 0x99;
        break;

    case 27:
        return SidOscOut(2) >> 4;

    case 28:
        return SidEnvOut(2);

    default:
        return last_reg_write_value;
    }
    return 0;
}

inline int  SidFilterOut()
{
	if (!filter_on)
    {
		if(enable_audio_out)
        	return ((vnf + mixer_dc) * (int)(volume));
		else return 0;
    }

    int vf;

    switch(hp_bp_lp)
    {
    default:

    case 0x00:
        vf = 0;
        break;

    case 0x01:
        vf = vlp;
        break;

    case 0x02:
        vf = vbp;
        break;

    case 0x03:
        vf = vlp + vbp;
        break;

    case 0x04:
        vf = vhp;
        break;

    case 0x05:
        vf = vlp + vhp;
        break;

    case 0x06:
        vf = vbp + vhp;
        break;

    case 0x07:
        vf = vlp + vbp + vhp;
        break;
    }

	if(enable_audio_out)
    	return ((vnf + vf + mixer_dc) * (int)(volume));
	else return 0;
}

inline int SidVoiceOutput(int voice_nr)
{
    return (SidOscOut(voice_nr) - wave_zero) * SidEnvOut(voice_nr) + voice_dc;
}

inline uint SidWaveDreieck(VOICE* v, VOICE* vs)
{
    static uint tmp;

    tmp = (v->ring_bit ? v->accu ^ vs->accu : v->accu)& 0x800000;
    return ((tmp ? ~v->accu : v->accu) >> 11) & 0xfff;
}

inline uint SidWaveSaegezahn(VOICE *v)
{
    return v->accu >> 12;
}

inline uint SidWaveRechteck(VOICE *v)
{
    return (v->test_bit || (v->accu >> 12) >= v->dutycycle) ? 0xfff:0x000;
}

inline uint SidWaveRauschen(VOICE *v)
{
    return ((v->shiftreg & 0x400000)>>11) | ((v->shiftreg & 0x100000)>>10) | ((v->shiftreg & 0x010000)>>7) | ((v->shiftreg & 0x002000)>>5) | ((v->shiftreg & 0x000800)>>4) | ((v->shiftreg & 0x000080)>>1) | ((v->shiftreg & 0x000010)<<1) | ((v->shiftreg & 0x000004)<< 2);
}

inline uint SidOscOut(int voice_nr)
{
	if(!voices[voice_nr].osc_enable) return 0;

	VOICE* 	v 	= &voices[voice_nr];
    VOICE*	vs 	= &voices[v->osc_source];

    switch (v->waveform)
    {
    case 0x01:
        return SidWaveDreieck(v,vs);

    case 0x02:
        return SidWaveSaegezahn(v);

    case 0x03:
        return wave0[SidWaveSaegezahn(v)]<<4;

    case 0x04:
        return (v->test_bit || (v->accu>>12) >= v->dutycycle) ? 0xfff:0x000;

    case 0x05:
        return (wave1[SidWaveDreieck(v,vs)>>1]<<4) & SidWaveRechteck(v);

    case 0x06:
        return (wave2[SidWaveSaegezahn(v)]<<4) & SidWaveRechteck(v);

    case 0x07:
        return (wave3[SidWaveSaegezahn(v)]<<4) & SidWaveRechteck(v);

    case 0x08:
        return SidWaveRauschen(v);

    default:
        return 0;
    }
}

inline uint SidEnvOut(int voice_nr)
{
    if(!voices[voice_nr].osc_enable) return 0;
    return voices[voice_nr].env_counter;
}

inline void SidCycle(int cycles_count)
{
	// OSC
	static unsigned int AccuPrev;
	static unsigned int AccuDelta;
	static unsigned int ShiftPeriod;
	static unsigned int Bit0;

	for(int i=0; i<3; i++)
	{
		// OSC Cycles
		if(!voices[i].test_bit)
		{
			AccuPrev = voices[i].accu;
			AccuDelta = cycles_count * voices[i].frequency;
			voices[i].accu += AccuDelta;
			voices[i].accu &= 0xffffff;
			voices[i].msb_rising =! (AccuPrev&0x800000) && (voices[i].accu & 0x800000);
			ShiftPeriod = 0x100000;
			while (AccuDelta)
			{
				if (AccuDelta < ShiftPeriod)
				{
					ShiftPeriod=AccuDelta;
					if(ShiftPeriod<=0x80000)
					{
						if (((voices[i].accu-ShiftPeriod) & 0x080000) || !(voices[i].accu & 0x080000))
						{
							break;
						}
					}
					else
					{
						if (((voices[i].accu - ShiftPeriod) & 0x080000) && !(voices[i].accu & 0x080000))
						{
							break;
						}
					}
				}

				Bit0 = ((voices[i].shiftreg>>22) ^ (voices[i].shiftreg>>17)) & 0x01;
				voices[i].shiftreg <<= 1;
				voices[i].shiftreg &= 0x7fffff;
				voices[i].shiftreg |= Bit0;
				AccuDelta -= ShiftPeriod;
			}
		}
	}

	for(int i=0; i<3; i++)
	{
		// ENV Cycles
		int rate_step = voices[i].rate_period - voices[i].rate_counter;
		if (rate_step <= 0)
		{
			rate_step += 0x7fff;
		}

		while (cycles_count)
		{
			if (cycles_count < rate_step)
			{
				voices[i].rate_counter += cycles_count;
				if (voices[i].rate_counter & 0x8000)
				{
					voices[i].rate_counter++;
					voices[i].rate_counter &= 0x7fff;
				}
				goto L10;
			}
			voices[i].rate_counter = 0;
			cycles_count -= rate_step;

			if(voices[i].state == ATTACK || ++voices[i].exponential_counter == voices[i].exponential_counter_period)
			{
				voices[i].exponential_counter = 0;
				if (voices[i].hold_zero)
				{
					rate_step = voices[i].rate_period;
					continue;
				}

				switch(voices[i].state)
				{
				case ATTACK:
					voices[i].env_counter++;
					voices[i].env_counter &= 0xff;
					if(voices[i].env_counter == 0xff)
					{
						voices[i].state = DECAY_SUSTAIN;
						voices[i].rate_period = RateCounterPeriod[voices[i].decay];
					}
					break;

				case DECAY_SUSTAIN:
					if(voices[i].env_counter != SustainLevel[voices[i].sustain]) --voices[i].env_counter;
					break;

				case RELEASE:
					voices[i].env_counter--;
					voices[i].env_counter &= 0xFF;
					break;
				}
				switch(voices[i].env_counter)
				{
				case 0xFF:
					voices[i].exponential_counter_period = 1;
					break;

				case 0x5D:
					voices[i].exponential_counter_period = 2;
					break;

				case 0x36:
					voices[i].exponential_counter_period = 4;
					break;

				case 0x1A:
					voices[i].exponential_counter_period = 8;
					break;

				case 0x0E:
					voices[i].exponential_counter_period = 16;
					break;

				case 0x06:
					voices[i].exponential_counter_period = 30;
					break;

				case 0x00:
					voices[i].exponential_counter_period = 1;
					voices[i].hold_zero = true;
					break;
				}
			}
			rate_step = voices[i].rate_period;
		}
		L10:;
	}

	// FILTER Cycles

	int voice1 = SidVoiceOutput(0);
	int voice2 = SidVoiceOutput(1);
	int voice3 = SidVoiceOutput(2);

	voice1 >>= 7;
	voice2 >>= 7;

	if(voice3_off && !(filter_key & 0x04)) voice3 = 0;
	else voice3 >>= 7;

	ext_in >>= 7;

	if(!filter_on)
	{
		vnf = voice1 + voice2 + voice3 + ext_in;
		vhp = vbp = vlp = 0;

	}
	else
	{
		int Vi;

		switch (filter_key)
		{

		default:

		case 0x00:
			Vi = 0;
			vnf = voice1 + voice2 + voice3 + ext_in;
			break;

		case 0x01:
			Vi = voice1;
			vnf = voice2 + voice3 + ext_in;
			break;

		case 0x02:
			Vi = voice2;
			vnf = voice1 + voice3 + ext_in;
			break;

		case 0x03:
			Vi = voice1 + voice2;
			vnf = voice3 + ext_in;
			break;

		case 0x04:
			Vi = voice3;
			vnf = voice1 + voice2 + ext_in;
			break;

		case 0x05:
			Vi = voice1 + voice3;
			vnf = voice2 + ext_in;
			break;

		case 0x06:
			Vi = voice2 + voice3;
			vnf = voice1 + ext_in;
			break;

		case 0x07:
			Vi = voice1 + voice2 + voice3;
			vnf = ext_in;
			break;

		case 0x08:
			Vi = ext_in;
			vnf = voice1 + voice2 + voice3;
			break;

		case 0x09:
			Vi = voice1 + ext_in;
			vnf = voice2 + voice3;
			break;

		case 0x0A:
			Vi = voice2 + ext_in;
			vnf = voice1 + voice3;
			break;

		case 0x0B:
			Vi = voice1 + voice2 + ext_in;
			vnf = voice3;
			break;

		case 0x0C:
			Vi = voice3 + ext_in;
			vnf = voice1 + voice2;
			break;

		case 0x0D:
			Vi = voice1 + voice3 + ext_in;
			vnf = voice2;
			break;

		case 0x0E:
			Vi = voice2 + voice3 + ext_in;
			vnf = voice1;
			break;

		case 0x0F:
			Vi = voice1 + voice2 + voice3 + ext_in;
			vnf = 0;
			break;
		}

		int delta_t_flt = 8;
		while (cycles_count)
		{
			if (cycles_count < delta_t_flt)
			{
				delta_t_flt = cycles_count;
			}
			int w0_delta_t = w0_ceil_dt*delta_t_flt>>6;
			int dVbp = (w0_delta_t*vhp >> 14);
			int dVlp = (w0_delta_t*vbp >> 14);
			vbp-=dVbp;
			vlp-=dVlp;
			vhp=(vbp*_1024_div_Q>>10)-vlp-Vi;
			cycles_count -= delta_t_flt;
		}
	}
}

////////// FILTER //////////

inline void SidSetW0()
{
    const float pi = 3.1415926535897932385;
    w0 = (int)(2 * pi * f0[filter_frequency] * 1.048576);
    const int w0_max_1 = (int)(2 * pi * 16000 * 1.048576);
    w0_ceil_1 = w0 <= w0_max_1 ? w0 : w0_max_1;
    const int w0_max_dt = (int)(2 * pi * 4000 * 1.048576);
    w0_ceil_dt = w0 <= w0_max_dt ? w0 : w0_max_dt;
}

inline void SidSetQ()
{
    _1024_div_Q = (int)(1024.0 / (0.707 + 1.0 * filter_resonance / 0x0F));
}
