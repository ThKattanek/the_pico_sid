//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: filter.h                              //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
//////////////////////////////////////////////////

#ifndef FILTER_H
#define FILTER_H

//// Sid Typen ////
#define MOS_6581 0
#define MOS_8580 1

#include "osc.h"
#include "env.h"

typedef int fc_point[2];

struct FILTER
{
	int			SidModel;
	bool		FilterOn;

	uint		VoiceDC;
    uint		WaveZero;

	uint		FilterKey;
    uint		FilterFrequenz;
    uint		FilterResonanz;
    uint		Voice3Off;
    uint		HpBpLp;
    uint		Volume;

	int     	MixerDC;
	int			Vhp;
    int			Vbp;
    int			Vlp;
    int			Vnf;
	int			w0,w0_ceil_1,w0_ceil_dt;
    int			_1024_div_Q;
    int*		f0;
	//fc_point*	f0_points;
    //int			f0_count;
}typedef FILTER;

void FilterSetSidModel(FILTER* filter, int sid_model);
void FilterInit(FILTER* filter);
void FilterReset(FILTER* filter);

void FilterOneCycle(FILTER* filter, OSC* osc, ENV* env);
void FilterExcecuteCycles(FILTER* filter, int cycles, int voice1, int voice2, int voice3, int ext_in);

void FilterEnableFilter(FILTER* filter, bool enabled);
int FilterGetOutput(FILTER* filter);
void FilterSetFrequenz(FILTER* filter, uint16_t filter_frequenz);
void FilterSetControl1(FILTER* filter, uint8_t value);
void FilterSetControl2(FILTER* filter, uint8_t value);
void FilterSetW0(FILTER* filter);
void FilterSetQ(FILTER* filter);

#endif // FILTER_H