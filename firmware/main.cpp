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

#pragma GCC optimize( "Ofast", "omit-frame-pointer", "modulo-sched", "modulo-sched-allow-regmoves", "gcse-sm", "gcse-las", "inline-small-functions", "delete-null-pointer-checks", "expensive-optimizations" ) 

#include <cstdio>
#include <malloc.h>
#include <pico/stdio.h>
#include <pico/stdlib.h>
#include <hardware/vreg.h>
#include <pico/multicore.h>
#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/pwm.h>
#include <hardware/adc.h>

#include "write_sid_reg.pio.h"
#include "read_sid_reg.pio.h"
#include "dma_read.pio.h"

#include "pico_sid.h"

#include "version.h"

// PIN MAPPING
#define RES_PIN 2
#define CLK_PIN 3		// Atention: When change this gpio, then also change this in write_sid_reg.pio
#define CS_PIN 4
#define RW_PIN 5
#define ADRR_PIN 6
#define DATA_PIN 11
#define AUDIO_PIN 28
#define ADC0_COMPARE_PIN 19
#define ADC1_COMPARE_PIN 20
#define ADC_2KHz_PIN 21

#define PICO_LED_PIN 25

#define ADC_OFFSET -5

volatile bool reset_state = true;

volatile PIO pio;
volatile uint sm0;	// write sid
volatile uint sm1;	// read sid
volatile uint sm2;	// dma read

volatile uint slice_num;

uint8_t* sid_io;

PICO_SID sid;

void InitPWMAudio(uint audio_out_gpio);
void DmaReadInit(PIO pio, uint sm, uint8_t* base_address);

void C64Reset(uint gpio, uint32_t events) 
{
    if (events & GPIO_IRQ_EDGE_FALL) 
	{
		// C64 Reset
		// Normalerweise erst wenn RESET 10 Zyklen auf Lo war
        sid.Reset();
		printf("Reset.");
    }
	else if (events & GPIO_IRQ_EDGE_RISE) 
	{
		// C64 Reset Ende
    }
}

void WriteSidReg()
{
	if (pio0_hw->irq & 1) 
	{
		pio0_hw->irq = 1;

		uint16_t incomming = pio->rxf[sm0];
		sid.WriteReg(incomming >> 2, (incomming >> 7) & 0xff);
	}
}

void Core1Entry() 
{
	InitPWMAudio(AUDIO_PIN);

    while (1)
	{
	}
}

void GetVersionNumber(uint8_t *major, uint8_t *minor, uint8_t *patch)
{
	*major = VERSION_MAJOR;
	*minor = VERSION_MINOR;
	*patch = VERSION_PATCH;
}

int main() 
{		
	vreg_set_voltage( VREG_VOLTAGE_1_30 );
	sleep_ms(100);
	set_sys_clock_khz(300000, true);
	stdio_init_all();	

	// Pico LED
	gpio_init(PICO_LED_PIN);
	gpio_set_dir(PICO_LED_PIN, true);
	gpio_put(PICO_LED_PIN, true);

	// ADC
	gpio_init(ADC_2KHz_PIN);
	gpio_set_dir(ADC_2KHz_PIN, true);	// Output

	gpio_init(ADC0_COMPARE_PIN);
	gpio_init(ADC1_COMPARE_PIN);

	gpio_set_dir(ADC0_COMPARE_PIN, false);	// Input
	gpio_set_dir(ADC1_COMPARE_PIN, false);	// Input

	// Init SID
	// memory for the sid io

	sid_io = (uint8_t*)memalign(32,32);
	
	// UART Start Message PicoSID
	printf("ThePicoSID by Thorsten Kattanek\n");

	uint8_t v_major, v_minor, v_patch;
	GetVersionNumber(&v_major, &v_minor, &v_patch);

	printf("Firmware Version: %d.%d.%d\n", v_major, v_minor, v_patch);
	
	// PIO Program initialize
	pio = pio0;

	// PIO Write SID
	uint offset = pio_add_program(pio, &write_sid_reg_program);
	sm0 = pio_claim_unused_sm(pio, true);
	write_sid_reg_program_init(pio, sm0, offset, CLK_PIN, CS_PIN);	//CLK_PIN + RW_PIN + A0-A4 + D0-D7 all PIN Count is 15
	irq_set_exclusive_handler(PIO0_IRQ_0, WriteSidReg);
	irq_set_enabled(PIO0_IRQ_0, true);
	pio0_hw->inte0 = PIO_IRQ0_INTE_SM0_BITS;

	// PIO Read SID
	offset = pio_add_program(pio, &read_sid_reg_program);
	sm1 = pio_claim_unused_sm(pio, true);
	read_sid_reg_program_init(pio, sm1, offset, CLK_PIN, CS_PIN, DATA_PIN);

	// PIO DMA READ
	offset = pio_add_program(pio, &dma_read_program);
	sm2 = pio_claim_unused_sm(pio, true);
	dma_read_program_init(pio, sm2, offset, ADRR_PIN, DATA_PIN, sid_io);
	DmaReadInit(pio, sm2, sid_io);

	// IRQ für die RESET Leitung
	gpio_init(RES_PIN);
    gpio_set_dir(RES_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(RES_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &C64Reset);

	// Start Core#1 for SID Emualtion
	multicore_launch_core1(Core1Entry);

	volatile uint16_t counter;
	volatile bool	adc0_compare_state;
	volatile bool	adc0_compare_state_old;
	volatile bool	adc1_compare_state;
	volatile bool	adc1_compare_state_old;

	sid.SetSidType(MOS_8580);

	for(int i=0; i<32; i++)
	{
		sid.WriteReg(i, 0);
		sid_io[i] = 0;
	}

    while (1)
    {
		// ADC
		sleep_us(1);

		counter++;
		gpio_put(ADC_2KHz_PIN, counter & 0x100);	// Create 2KHz Signal for the mosfet

		// AX
		adc0_compare_state = gpio_get(ADC0_COMPARE_PIN);
		if(adc0_compare_state && !adc0_compare_state_old)
			sid_io[25] = (counter + ADC_OFFSET) & 0xff;
		adc0_compare_state_old = adc0_compare_state;

		// AY
		adc1_compare_state = gpio_get(ADC1_COMPARE_PIN);
		if(adc1_compare_state && !adc1_compare_state_old)
			sid_io[26] = (counter + ADC_OFFSET) & 0xff;
		adc1_compare_state_old = adc1_compare_state;
    }
}

void pwm_irq_handle()
{
	pwm_clear_irq(slice_num);

	for(int i=0; i<6; i++) 	// 300MHz
	{
		sid.Clock(4);			
		sid_io[0x1b] = sid.voice[2].wave.ReadOSC();
		sid_io[0x1c] = sid.voice[2].envelope.ReadEnv();
	}

	//uint16_t out = ((sid[0].AudioOut() >> 4) + 32768) / (float)0xffff * 0x7ff;	// Version 0.1.0	    
	uint16_t out = sid.AudioOut(11) + 1024; 										// Version 0.2.0

	pwm_set_gpio_level(AUDIO_PIN, out);
}

void InitPWMAudio(uint audio_out_gpio)
{
	gpio_set_function(audio_out_gpio, GPIO_FUNC_PWM);
	gpio_set_drive_strength(audio_out_gpio, GPIO_DRIVE_STRENGTH_2MA);

	// Find out which PWM slice is connected to GPIO 2 (it's slice 1)
	slice_num = pwm_gpio_to_slice_num(audio_out_gpio);

	// Set pwm frequenz
	//pwm_set_clkdiv_int_frac(slice_num, 2,15);	// PWM Frequency of 41223Hz when Systemclock is 248MHz.
	pwm_set_clkdiv_int_frac(slice_num, 3, 9);	// PWM Frequency of 41118Hz when Systemclock is 300MHz.

	// Set period of 4 cycles (0 to 3 inclusive)
	pwm_set_wrap(slice_num, 0x07ff);	// 11Bit

	// Set channel A output high for one cycle before dropping
	pwm_set_chan_level(slice_num, PWM_CHAN_A, 0x0400);

	// Set the PWM running
	pwm_set_enabled(slice_num, true);

	// IRQ Swt
	pwm_clear_irq(slice_num);
	pwm_set_irq_enabled(slice_num, true);
	irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_irq_handle);
	irq_set_enabled(PWM_IRQ_WRAP, true);
}

// Set up DMA channels for handling reads
void DmaReadInit(PIO pio, uint sm, uint8_t* base_address) 
{
    // Read channel: copy requested byte to TX fifo (source address set by write channel below)
    uint read_channel = 0;
    dma_channel_claim(read_channel);

    dma_channel_config read_config = dma_channel_get_default_config(read_channel);
    channel_config_set_read_increment(&read_config, false);
    channel_config_set_write_increment(&read_config, false);
    channel_config_set_dreq(&read_config, pio_get_dreq(pio, sm, true));
    channel_config_set_transfer_data_size(&read_config, DMA_SIZE_8);

    dma_channel_configure(read_channel,
                          &read_config,
                          &pio->txf[sm], // write to TX fifo
                          base_address,  // read from base address (overwritten by write channel)
                          1,             // transfer count
                          false);        // start later

    // Write channel: copy address from RX fifo to the read channel's READ_ADDR_TRIGGER
    uint write_channel = 1;
    dma_channel_claim(write_channel);
    dma_channel_config write_config = dma_channel_get_default_config(write_channel);
    channel_config_set_read_increment(&write_config, false);
    channel_config_set_write_increment(&write_config, false);
    channel_config_set_dreq(&write_config, pio_get_dreq(pio, sm, false));
    channel_config_set_transfer_data_size(&write_config, DMA_SIZE_32);

    volatile void *read_channel_addr = &dma_channel_hw_addr(read_channel)->al3_read_addr_trig;
    dma_channel_configure(write_channel,
                          &write_config,
                          read_channel_addr, // write to read_channel READ_ADDR_TRIGGER
                          &pio->rxf[sm],     // read from RX fifo
                          0xffffffff,        // do many transfers
                          true);             // start now
}