//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid.h                                 //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
//////////////////////////////////////////////////

#ifndef	SID_H
#define	SID_H

#if defined(__linux__) || defined(_WIN32)

#include <stdint.h>
#include <stdbool.h>
typedef unsigned int uint;

#else

#include <pico/stdlib.h>

#endif

#include "f0_points_6581.h"
#include "f0_points_8580.h"

//// Sid Typen ////
#define MOS_6581 		0
#define MOS_8580 		1

#define	ATTACK			0
#define	DECAY_SUSTAIN   1
#define RELEASE			2

#define	PUFFER_TIME		1
struct VOICE
{
	/// Oscillator Register ///
    bool			osc_enable;
    bool			test_bit;
    bool			ring_bit;
    bool			sync_bit;
    uint8_t			waveform;
    uint			frequency;
    uint			dutycycle;
    uint			accu;
    uint			shiftreg;
    uint			msb_rising;
    int				osc_source;
    int				osc_destination;

    /// Envelope Register ///
    bool			key_bit;
    int				state;
    uint			rate_period;
    uint			rate_counter;
    uint			exponential_counter_period;
    uint			exponential_counter;
    uint			env_counter;
    bool			hold_zero;
    uint			attack;
    uint			decay;
    uint			sustain;
    uint			release;
}typedef VOICE;

void SidInit();
void SidReset();
void SidSetSamplerate(uint samplerate);
void SidSetChipTyp(int chip_type);
void SidWriteReg(uint16_t address, uint8_t value);
int  SidVoiceOutput(int voice_nr);
int  SidTestOut();
int  SidFilterOut();
uint SidWaveDreieck(VOICE* v, VOICE* vs);
uint SidWaveSaegezahn(VOICE *v);
uint SidWaveRechteck(VOICE *v);
uint SidWaveRauschen(VOICE *v);
uint SidOscOut(int voice_nr);
uint SidEnvOut(int voice_nr);
bool SidCycle(int cycles_count);
void SidSetW0();
void SidSetQ();

#endif // SID_H
