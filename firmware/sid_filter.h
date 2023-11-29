//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid_filter.h                          //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
// The template used was resid:                 //
// https://github.com/libsidplayfp/resid        //
//                                              //
//////////////////////////////////////////////////

#ifndef SID_FILTER_CLASS_H
#define SID_FILTER_CLASS_H

#include <stdint.h>

#include "./pico_sid_defs.h"
#include "./spline.h"

class SID_FILTER
{
public:
    SID_FILTER();
    ~SID_FILTER();

	void EnableFilter(bool enable);

    void SetSidType(sid_type type);
    void Reset();

    void WriteFcLo(reg8 value);
    void WriteFcHi(reg8 value);
    void WriteResFilt(reg8 value);
    void WriteModeVol(reg8 value);

	void SetW0();
	void SetQ();
	void FcDefault(const fc_point*& points, int& count);
	PointPlotter<int> FcPlotter();

    void Clock(cycle_count delta_t, int voice1, int voice2, int voice3, int ext_in);
    int Output();

	// Spline functions.
  	void fc_default(const fc_point*& points, int& count);
  	PointPlotter<int> fc_plotter();

protected:
	void set_w0();
	void set_Q();

	// Filter enabled.
	bool enabled;

	// Filter cutoff frequency.
	reg12 fc;

	// Filter resonance.
	reg8 res;

	// Selects which inputs to route through filter.
	reg8 filt;

	// Switch voice 3 off.
	reg8 voice3off;

	// Highpass, bandpass, and lowpass filter modes.
	reg8 hp_bp_lp;

	// Output master volume.
	reg4 vol;

	// Mixer DC offset.
	int mixer_DC;

	// State of filter.
	int Vhp; // highpass
	int Vbp; // bandpass
	int Vlp; // lowpass
	int Vnf; // not filtered

	// Cutoff frequency, resonance.
	int w0, w0_ceil_1, w0_ceil_dt;
	int _1024_div_Q;

	// Cutoff frequency tables.
	// FC is an 11 bit register.
	int f0_6581[2048];
	int f0_8580[2048];
	int* f0;
	static fc_point f0_points_6581[];
	static fc_point f0_points_8580[];
	fc_point* f0_points;
	int f0_count;		

	friend class PICO_SID;		
};

inline void SID_FILTER::Clock(cycle_count delta_t, int voice1, int voice2, int voice3, int ext_in)
{
	// Scale each voice down from 20 to 13 bits.
	voice1 >>= 7;
	voice2 >>= 7;

	// NB! Voice 3 is not silenced by voice3off if it is routed through
	// the filter.
	if (voice3off && !(filt & 0x04)) {
	voice3 = 0;
	}
	else {
	voice3 >>= 7;
	}

	ext_in >>= 7;

	// Enable filter on/off.
	// This is not really part of SID, but is useful for testing.
	// On slow CPUs it may be necessary to bypass the filter to lower the CPU
	// load.
	if (!enabled) {
	Vnf = voice1 + voice2 + voice3 + ext_in;
	Vhp = Vbp = Vlp = 0;
	return;
	}

	// Route voices into or around filter.
	// The code below is expanded to a switch for faster execution.
	// (filt1 ? Vi : Vnf) += voice1;
	// (filt2 ? Vi : Vnf) += voice2;
	// (filt3 ? Vi : Vnf) += voice3;

	int Vi;

	switch (filt) {
	default:
	case 0x0:
	Vi = 0;
	Vnf = voice1 + voice2 + voice3 + ext_in;
	break;
	case 0x1:
	Vi = voice1;
	Vnf = voice2 + voice3 + ext_in;
	break;
	case 0x2:
	Vi = voice2;
	Vnf = voice1 + voice3 + ext_in;
	break;
	case 0x3:
	Vi = voice1 + voice2;
	Vnf = voice3 + ext_in;
	break;
	case 0x4:
	Vi = voice3;
	Vnf = voice1 + voice2 + ext_in;
	break;
	case 0x5:
	Vi = voice1 + voice3;
	Vnf = voice2 + ext_in;
	break;
	case 0x6:
	Vi = voice2 + voice3;
	Vnf = voice1 + ext_in;
	break;
	case 0x7:
	Vi = voice1 + voice2 + voice3;
	Vnf = ext_in;
	break;
	case 0x8:
	Vi = ext_in;
	Vnf = voice1 + voice2 + voice3;
	break;
	case 0x9:
	Vi = voice1 + ext_in;
	Vnf = voice2 + voice3;
	break;
	case 0xa:
	Vi = voice2 + ext_in;
	Vnf = voice1 + voice3;
	break;
	case 0xb:
	Vi = voice1 + voice2 + ext_in;
	Vnf = voice3;
	break;
	case 0xc:
	Vi = voice3 + ext_in;
	Vnf = voice1 + voice2;
	break;
	case 0xd:
	Vi = voice1 + voice3 + ext_in;
	Vnf = voice2;
	break;
	case 0xe:
	Vi = voice2 + voice3 + ext_in;
	Vnf = voice1;
	break;
	case 0xf:
	Vi = voice1 + voice2 + voice3 + ext_in;
	Vnf = 0;
	break;
  }

  // Maximum delta cycles for the filter to work satisfactorily under current
  // cutoff frequency and resonance constraints is approximately 8.
  cycle_count delta_t_flt = 8;

  while (delta_t) {
    if (delta_t < delta_t_flt) {
      delta_t_flt = delta_t;
    }

    // delta_t is converted to seconds given a 1MHz clock by dividing
    // with 1 000 000. This is done in two operations to avoid integer
    // multiplication overflow.

    // Calculate filter outputs.
    // Vhp = Vbp/Q - Vlp - Vi;
    // dVbp = -w0*Vhp*dt;
    // dVlp = -w0*Vbp*dt;
    int w0_delta_t = w0_ceil_dt*delta_t_flt >> 6;

    int dVbp = (w0_delta_t*Vhp >> 14);
    int dVlp = (w0_delta_t*Vbp >> 14);
    Vbp -= dVbp;
    Vlp -= dVlp;
    Vhp = (Vbp*_1024_div_Q >> 10) - Vlp - Vi;

    delta_t -= delta_t_flt;
  }
}


// ----------------------------------------------------------------------------
// SID audio output (20 bits).
// ----------------------------------------------------------------------------

inline int SID_FILTER::Output()
{
	// This is handy for testing.
	if (!enabled) {
	return (Vnf + mixer_DC)*static_cast<int>(vol);
	}

	// Mix highpass, bandpass, and lowpass outputs. The sum is not
	// weighted, this can be confirmed by sampling sound output for
	// e.g. bandpass, lowpass, and bandpass+lowpass from a SID chip.

	// The code below is expanded to a switch for faster execution.
	// if (hp) Vf += Vhp;
	// if (bp) Vf += Vbp;
	// if (lp) Vf += Vlp;

	int Vf;

	switch (hp_bp_lp) {
	default:
	case 0x0:
	Vf = 0;
	break;
	case 0x1:
	Vf = Vlp;
	break;
	case 0x2:
	Vf = Vbp;
	break;
	case 0x3:
	Vf = Vlp + Vbp;
	break;
	case 0x4:
	Vf = Vhp;
	break;
	case 0x5:
	Vf = Vlp + Vhp;
	break;
	case 0x6:
	Vf = Vbp + Vhp;
	break;
	case 0x7:
	Vf = Vlp + Vbp + Vhp;
	break;
	}

	// Sum non-filtered and filtered output.
	// Multiply the sum with volume.
	return (Vnf + Vf + mixer_DC)*static_cast<int>(vol);
}
#endif // SID_FILTER_CLASS_H
