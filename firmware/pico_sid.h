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

class PICO_SID
{
public:
	PICO_SID();
	~PICO_SID();

	void SetSidType(sid_type type);
    void Clock(cycle_count delta_t);

	void Reset();
    void WriteReg(uint8_t address, uint8_t value);
    void WriteReg();
	uint8_t ReadReg(uint8_t address);
    int AudioOut();

    sid_type sid_model;
    SID_VOICE voice[3];

    reg8 sid_register[0x20];

    reg8 write_address;
    reg8 databus_value;
    cycle_count write_pipeline;
};

#endif // PICO_SID_CLASS_H
