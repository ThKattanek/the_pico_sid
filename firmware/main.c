//////////////////////////////////////////////////
//                                              //
// ThePicoSID                                   //
// von Thorsten Kattanek                        //
//                                              //
// #file: main.c                                //
//                                              //
// https://github.com/ThKattanek/the_pico_sid   //
//                                              //
//////////////////////////////////////////////////

#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <hardware/clocks.h>
#include <pico/binary_info.h>

#include "write_sid_reg.pio.h"

#include "./sid.h"

const uint RES_PIN = 2;
const uint CS_PIN = 3;
const uint CLK_PIN = 4;
const uint RW_PIN = 5;

const uint A0_PIN = 6;
const uint A1_PIN = 7;
const uint A2_PIN = 8;
const uint A3_PIN = 9;
const uint A4_PIN = 10;

volatile static bool reset_state = true;
int main() {
    bi_decl(bi_program_description("The Pico SID"));
    bi_decl(bi_1pin_with_name(PICO_DEFAULT_LED_PIN, "On-board LED"));
    bi_decl(bi_1pin_with_name(RES_PIN, "C64 Reset"));
    bi_decl(bi_1pin_with_name(CLK_PIN, "C64 Clock"));
    bi_decl(bi_1pin_with_name(RW_PIN, "C64 SID RW"));
    bi_decl(bi_1pin_with_name(CS_PIN, "C64 SID Chip Select"));

	set_sys_clock_khz(125000, true);

    stdio_init_all();	

	// PIO Program initialize
	PIO pio = pio0;

	uint offset = pio_add_program(pio, &write_sid_reg_program);
	uint sm = pio_claim_unused_sm(pio, true);

	write_sid_reg_program_init(pio, sm, offset, CLK_PIN, CS_PIN);	//CLK_PIN + RW + A0-A4 +
	
	printf("The Pico SID by Thorsten Kattanek\n");
 
    SidReset();

	uint8_t old = 0;

    while (1)
    {
		while (!(pio->irq & (1<<0))); 		// wait for irq 1 fro either of the pio programs
    		pio_interrupt_clear(pio, 0); 	// clear the irq so we can wait for it again
		
		// uint16_t value = pio->rxf[sm]; 

		uint8_t value = (pio->rxf[sm] >> 7) & 0xff; 

		if(((old+1)&0xff) != value)
			printf("ERROR%2.2X\n", value);
		old = value;

		//printf("SID REG: %2.2X <-'%d'\n", (value >> 2) & 0x1f, (value >> 7) & 0xff);
    }
}
