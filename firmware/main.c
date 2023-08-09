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
const uint CLK_PIN = 3;
const uint RW_PIN = 4;
const uint CS_PIN = 5;

volatile static bool reset_state = true;
int main() {
    bi_decl(bi_program_description("The Pico SID"));
    bi_decl(bi_1pin_with_name(PICO_DEFAULT_LED_PIN, "On-board LED"));
    bi_decl(bi_1pin_with_name(RES_PIN, "C64 Reset"));
    bi_decl(bi_1pin_with_name(CLK_PIN, "C64 Clock"));
    bi_decl(bi_1pin_with_name(RW_PIN, "C64 SID RW"));
    bi_decl(bi_1pin_with_name(CS_PIN, "C64 SID Chip Select"));

    stdio_init_all();	

	// PIO Program initialize
	PIO pio = pio0;

	uint offset = pio_add_program(pio, &write_sid_reg_program);
	uint sm = pio_claim_unused_sm(pio, true);

	write_sid_reg_program_init(pio, sm, offset, CLK_PIN, CS_PIN);
	
	printf("The Pico SID by Thorsten Kattanek\n");
 
    SidReset();

	uint t = 0;

    while (1)
    {
		while (!(pio->irq & (1<<0))); // wait for irq 0 fro either of the pio programs
    		pio_interrupt_clear(pio, 0); // clear the irq so we can wait for it again
		printf("IRQ_0%d\n",t++);
    }
}
