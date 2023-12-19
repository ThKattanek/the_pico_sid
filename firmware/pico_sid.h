//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: pico_sid.h                            //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
// The template used was resid:                 //
// https://github.com/libsidplayfp/resid        //
//                                              //
////////////////////////////////////////////////// 

#ifndef PICO_SID_CLASS_H
#define PICO_SID_CLASS_H

#include <stdint.h>

#include "./pico_sid_defs.h"
#include "./sid_voice.h"
#include "./sid_filter.h"
#include "./sid_extfilter.h"

class PICO_SID
{
public:
	PICO_SID();
	~PICO_SID();

	void SetSidType(sid_type type);
	void EnableFilter(bool enable);
	void EnableExtFilter(bool enable);
	void EnableDigiBoost8580(bool enable);

    void Clock(cycle_count delta_t);
	void Reset();
    void WriteReg(uint8_t write_address, uint8_t bus_value);
	uint8_t ReadReg(uint8_t address);
    void Input(int sample);
    int AudioOut();
	int AudioOut(int bits);

	void SetExtIn();

    SID_VOICE voice[3];
    SID_FILTER filter;
	SID_EXTFILTER extfilter;

    reg8 sid_register[0x20];

    reg8 write_address;
    reg8 bus_value;

	sid_type sid_model;
	bool	filter_enable;
    bool	extfilter_enable;

	int		digi_boost_enable;			// 0=off 1=on (for 8580)
	int		digi_level[2][2];

    int     ext_in;
};

inline int PICO_SID::AudioOut()
{
    // Ausgabe der reinen Wellenform
    // return voice[0].wave.Output() + voice[1].wave.Output() + voice[2].wave.Output();

    // Ausgabe Amplituden moduliert
    // return ((voice[0].Output() + voice[1].Output() + voice[2].Output()) / (float)0x2ffffd) * 0xffff;

    // Ausgabe SID Filter
	// return (filter.Output()  / (float)1048576) * 0xffff;	// Audiolevel is lower
	return (extfilter.Output()  / (float)524287) * 0xffff;		// Audiolevel is higher
}

inline int PICO_SID::AudioOut(int bits)
{
	const int range = 1 << bits;
	const int half = range >> 1;
	int sample = extfilter.Output()/((4095*255 >> 7)*3*15*2/range);
	
	if (sample >= half)
    	return half - 1;
  
  	if (sample < -half)
    	return -half;
  
  	return sample;
}

inline void PICO_SID::SetExtIn()
{
	Input(digi_level[sid_model][digi_boost_enable]);
}

inline void PICO_SID::Clock(cycle_count delta_t)
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
    filter.Clock(delta_t, voice[0].Output(), voice[1].Output(), voice[2].Output(), ext_in);

    // Clock external filter.
    extfilter.Clock(delta_t, filter.Output());
}

#endif // PICO_SID_CLASS_H
