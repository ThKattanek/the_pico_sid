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
    // FIXME: No variable length arrays in ISO C++, hardcoding to max 12 bits.
    // double vbit[bits];
    double vbit[12];

    // Calculate voltage contribution by each individual bit in the R-2R ladder.
    for (int set_bit = 0; set_bit < bits; set_bit++) {
        int bit;

        double Vn = 1.0;          // Normalized bit voltage.
        double R = 1.0;           // Normalized R
        double _2R = _2R_div_R*R; // 2R
        double Rn = term ?        // Rn = 2R for correct termination,
                        _2R : INFINITY;         // INFINITY for missing termination.

        // Calculate DAC "tail" resistance by repeated parallel substitution.
        for (bit = 0; bit < set_bit; bit++) {
            if (Rn == INFINITY) {
                Rn = R + _2R;
            }
            else {
                Rn = R + _2R*Rn/(_2R + Rn); // R + 2R || Rn
            }
        }

        // Source transformation for bit voltage.
        if (Rn == INFINITY) {
            Rn = _2R;
        }
        else {
            Rn = _2R*Rn/(_2R + Rn);  // 2R || Rn
            Vn = Vn*Rn/_2R;
        }

        // Calculate DAC output voltage by repeated source transformation from
        // the "tail".
        for (++bit; bit < bits; bit++) {
            Rn += R;
            double I = Vn/Rn;
            Rn = _2R*Rn/(_2R + Rn);  // 2R || Rn
            Vn = Rn*I;
        }

        vbit[set_bit] = Vn;
    }

    // Calculate the voltage for any combination of bits by superpositioning.
    for (int i = 0; i < (1 << bits); i++) {
        int x = i;
        double Vo = 0;
        for (int j = 0; j < bits; j++) {
            Vo += (x & 0x1)*vbit[j];
            x >>= 1;
        }

        // Scale maximum output to 2^bits - 1.
        dac[i] = (unsigned short)(((1 << bits) - 1)*Vo + 0.5);
    }
}
