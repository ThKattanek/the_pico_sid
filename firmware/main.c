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
#include <pico/binary_info.h>

#include "./sid.h"

const uint LED_PIN = 25;

const uint RES_PIN = 2;
const uint CLK_PIN = 3;
const uint RW_PIN = 4;
const uint CS_PIN = 5;

volatile static bool reset_state = true;

void ResetPinCallback(uint gpio, uint32_t events)
{
    switch(gpio)
    {
    case RES_PIN:
        if(events & GPIO_IRQ_EDGE_RISE)
            reset_state = true;
        else if(events & GPIO_IRQ_EDGE_FALL)
            reset_state = false;
        break;

    case CS_PIN:
        if(events & GPIO_IRQ_EDGE_RISE)
        {
#ifndef NDEBUG
            printf("CS Signal is edge rise.\n");
#endif
        }

        if(events & GPIO_IRQ_EDGE_FALL)
        {
#ifndef NDEBUG
            printf("CS Signal is edge fall.\n");
#endif
        }
        break;

    case RW_PIN:
        if(events & GPIO_IRQ_EDGE_RISE)
        {
#ifndef NDEBUG
            printf("RW Signal is edge rise.\n");
#endif
        }

        if(events & GPIO_IRQ_EDGE_FALL)
        {
#ifndef NDEBUG
            printf("RW Signal is edge fall.\n");
#endif
        }
        break;
    }
}

int main() {
    bi_decl(bi_program_description("The Pico SID"));
    bi_decl(bi_1pin_with_name(LED_PIN, "On-board LED"));
    bi_decl(bi_1pin_with_name(RES_PIN, "C64 Reset"));
    bi_decl(bi_1pin_with_name(CLK_PIN, "C64 Clock"));
    bi_decl(bi_1pin_with_name(RW_PIN, "C64 SID RW"));
    bi_decl(bi_1pin_with_name(CS_PIN, "C64 SID Chip Select"));

    stdio_init_all();

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
    gpio_set_irq_enabled_with_callback(RES_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &ResetPinCallback);
    gpio_set_irq_enabled(CS_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(RW_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

    SidReset();

    while (1)
    {
        if(!reset_state)
        {
            SidReset();
#ifndef NDEBUG
            printf("SID RESET. \n");
#endif
        }
    }
}
