//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid_extfilter.h                       //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
// The template used was resid:                 //
// https://github.com/libsidplayfp/resid        //
//                                              //
//////////////////////////////////////////////////

#ifndef SID_EXTFILTER_CLASS_H
#define SID_EXTFILTER_CLASS_H

#include <stdint.h>

#include "./pico_sid_defs.h"

class SID_EXTFILTER
{
public:
    SID_EXTFILTER();

    void SetSidType(sid_type type);
	void EnableFilter(bool enable);
    void Reset();

	void Clock(cycle_count delta_t, int Vi);
	int Output();

protected:

	bool enabled;
	
	// Maximum mixer DC offset.
	
	int mixer_dc;
	int Vlp; // lowpass
  	int Vhp; // highpass
  	int Vo;

	// Cutoff frequencies.
  	int w0lp;
  	int w0hp;

	friend class PICO_SID;		
};

// ----------------------------------------------------------------------------
// SID clocking - delta_t cycles.
// ----------------------------------------------------------------------------
inline void SID_EXTFILTER::Clock(cycle_count delta_t, int Vi)
{
	// This is handy for testing.
	if (!enabled)
	{
    	// Remove maximum DC level since there is no filter to do it.
    	Vlp = Vhp = 0;
    	Vo = Vi - mixer_dc;
    	return;
	}

	// Maximum delta cycles for the external filter to work satisfactorily
  	// is approximately 8.
  	cycle_count delta_t_flt = 8;

  	while (delta_t) 
	{
    	if (delta_t < delta_t_flt) 
		{
      		delta_t_flt = delta_t;
    	}

    	// delta_t is converted to seconds given a 1MHz clock by dividing
    	// with 1 000 000.

    	// Calculate filter outputs.
    	// Vo  = Vlp - Vhp;
    	// Vlp = Vlp + w0lp*(Vi - Vlp)*delta_t;
    	// Vhp = Vhp + w0hp*(Vlp - Vhp)*delta_t;

    	int dVlp = (w0lp*delta_t_flt >> 8)*(Vi - Vlp) >> 12;
    	int dVhp = w0hp*delta_t_flt*(Vlp - Vhp) >> 20;
    	Vo = Vlp - Vhp;
    	Vlp += dVlp;
    	Vhp += dVhp;

    	delta_t -= delta_t_flt;
  	}
}

// ----------------------------------------------------------------------------
// Audio output (19.5 bits).
// ----------------------------------------------------------------------------
inline int SID_EXTFILTER::Output()
{
	return Vo;
}

#endif // SID_EXTFILTER_CLASS_H
