//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: sid_dac.cpp                           //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
// The template used was resid:                 //
// https://github.com/libsidplayfp/resid        //
//                                              //
//////////////////////////////////////////////////

#include "./sid_dac.h"
#include <math.h>

void BuildDacTable(unsigned short* dac, int bits, double _2R_div_R, bool term)
{
    double vbit[12];

    for (int set_bit = 0; set_bit < bits; set_bit++) {
        int bit;

        double Vn = 1.0;          // Normalized bit voltage.
        double R = 1.0;           // Normalized R
        double _2R = _2R_div_R*R; // 2R
        double Rn = term ?        // Rn = 2R for correct termination,
                        _2R : INFINITY;         // INFINITY for missing termination.

        for (bit = 0; bit < set_bit; bit++) {
            if (Rn == INFINITY) {
                Rn = R + _2R;
            }
            else {
                Rn = R + _2R*Rn/(_2R + Rn); // R + 2R || Rn
            }
        }

        if (Rn == INFINITY) {
            Rn = _2R;
        }
        else {
            Rn = _2R*Rn/(_2R + Rn);  // 2R || Rn
            Vn = Vn*Rn/_2R;
        }

        for (++bit; bit < bits; bit++) {
            Rn += R;
            double I = Vn/Rn;
            Rn = _2R*Rn/(_2R + Rn);  // 2R || Rn
            Vn = Rn*I;
        }

        vbit[set_bit] = Vn;
    }

    for (int i = 0; i < (1 << bits); i++) {
        int x = i;
        double Vo = 0;
        for (int j = 0; j < bits; j++) {
            Vo += (x & 0x1)*vbit[j];
            x >>= 1;
        }

        dac[i] = (unsigned short)(((1 << bits) - 1)*Vo + 0.5);
    }
}
