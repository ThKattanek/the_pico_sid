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

#include "read_sid_reg.pio.h"

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

	uint offset = pio_add_program(pio, &read_sid_reg_program);
	uint sm = pio_claim_unused_sm(pio, true);

	read_sid_reg_program_init(pio, sm, offset, PICO_DEFAULT_LED_PIN);
	pio_sm_set_enabled(pio, sm, true);

	uint32_t LED_FREQ = 10;
	pio->txf[sm] = (clock_get_hz(clk_sys) / (2 * LED_FREQ)) - 3;
	

/*
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    // Init Ext Reset
    gpio_set_dir(RES_PIN, GPIO_IN);
    // Init Ext Clock
    gpio_set_dir(CLK_PIN, GPIO_IN);
    // Init Ext RW
    gpio_set_dir(RW_PIN, GPIO_IN);
    // Init Ext CS
    gpio_set_dir(CS_PIN, GPIO_IN);

    // Init GPIO IRQ for RES_PIN
    //gpio_set_irq_enabled_with_callback(RES_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &ResetPinCallback);
    //gpio_set_irq_enabled(CS_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    //gpio_set_irq_enabled(RW_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

	gpio_set_irq_enabled_with_callback(CS_PIN,GPIO_IRQ_EDGE_RISE, true, &ResetPinCallback);
*/
	printf("The Pico SID by Thorsten Kattanek\n");
 
    SidReset();

    while (1)
    {
    }
}
