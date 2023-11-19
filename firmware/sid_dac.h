//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid_dac.h                             //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
// The template used was resid:                 //
// https://github.com/libsidplayfp/resid        //
//                                              //
//////////////////////////////////////////////////

#ifndef SID_DAC_H
#define SID_DAC_H

void BuildDacTable(unsigned short* dac, int bits, double _2R_div_R, bool term);

#endif // SID_DAC_H
