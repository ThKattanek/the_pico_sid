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
#include <cstring>
#include <pico/stdio.h>
#include <pico/stdlib.h>
#include <hardware/vreg.h>
#include <pico/multicore.h>
#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/pwm.h>
#include <hardware/adc.h>
#include <hardware/flash.h>

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

#define ADC_OFFSET -9	// OLD -5

volatile bool reset_state = true;
volatile bool adc_enable = true;

volatile PIO pio;
volatile uint sm0;	// write sid
volatile uint sm1;	// read sid
volatile uint sm2;	// dma read

volatile uint slice_num;

uint8_t* sid_io;

PICO_SID sid;

#define SYSTEM_CLOCK 279000

void InitPWMAudio(uint audio_out_gpio);
void DmaReadInit(PIO pio, uint sm, uint8_t* base_address);
uint8_t configuration[32];
volatile bool config_is_new = false;

#define FLASH_CONFIG_OFFSET (256 * 1024)
const uint8_t *flash_target_contents = (const uint8_t *)( XIP_BASE + FLASH_CONFIG_OFFSET );

#define CONFIG_01 11

void C64Reset(uint gpio, uint32_t events) 
{
    if (events & GPIO_IRQ_EDGE_FALL) 
	{
		// C64 Reset
		// Normalerweise erst wenn RESET 10 Zyklen auf Lo war
        sid.Reset();
    }
	else if (events & GPIO_IRQ_EDGE_RISE) 
	{
		// C64 Reset Ende
    }
}

void WriteConfig()
{
	set_sys_clock_khz(125000, true);
	sleep_ms(2);

	multicore_lockout_start_blocking();		// Halt Core1

	flash_range_erase( FLASH_CONFIG_OFFSET, FLASH_SECTOR_SIZE );
	flash_range_program( FLASH_CONFIG_OFFSET, configuration, FLASH_PAGE_SIZE );
	
	multicore_lockout_end_blocking();		// Weiter Core1

	set_sys_clock_khz(SYSTEM_CLOCK, true);
	sleep_ms(2);
}

void ReadConfig()
{
	set_sys_clock_khz(125000, true);
	sleep_ms(2);

	memcpy( configuration, flash_target_contents, 32 );

	if(0 != strcmp((const char*)configuration, "THEPICOSID"))
	{
		// Default Variablen
		printf("Keine Konfiguration gefunden!\n");
		strcpy((char*)configuration, "THEPICOSID");
		configuration[CONFIG_01] = 0x03;	// Default Config_1
		WriteConfig();
	}
	

	uint8_t value = configuration[CONFIG_01];
	if(value & 0x01)
		sid.SetSidType(MOS_8580);
	else	
		sid.SetSidType(MOS_6581);
	sid.EnableFilter(value & 0x02);
	sid.EnableExtFilter(value & 0x04);
	sid.EnableDigiBoost8580(value & 0x08);
	
	set_sys_clock_khz(SYSTEM_CLOCK, true);
	sleep_ms(2);
}

void ConfigOutput()
{
	// Output Coniguration to Serial
	printf("\n-Configuration-\n");
	if(sid.sid_model == MOS_6581)
		printf("SID Model is: MOS-6581\n");
	else
		printf("SID Model is: MOS-8580\n");
	
	printf("Filter is: ");
	if(sid.filter_enable)
		printf("on\n");
	else
		printf("off\n");

	printf("ExtFilter is: ");
	if(sid.extfilter_enable)
		printf("on\n");
	else
		printf("off\n");

	printf("Digiboost 8580 is: ");
	if(sid.digi_boost_enable)
		printf("on\n");
	else
		printf("off\n");
}

void CheckConfig(uint8_t address, uint8_t value)
{
	static bool is_ready = false;
	static bool is_command = false;
	static uint8_t last_command = 0x88;
	static char incomming_str[11] = {0,0,0,0,0,0,0,0,0,0,0};

	uint8_t val = 0;

	switch(address)
	{
		case 0x1d:
			if(!is_ready)
			{
			for(int i=0; i<9; i++)
				incomming_str[i] = incomming_str[i+1];
			incomming_str[9] = value;
				if(strcmp(incomming_str, "THEPICOSID") == 0)
					is_ready = true;
			}
			else 
			{
				if(!is_command)	
				{
					// check of command
					switch (value)
					{
					case 0x00:	// ThePicoSidCheck
						last_command = value;
						is_command = true;
						break;
					
					case 0x01:	// Config01_Write
						last_command = value;
						is_command = true;
						break;

					case 0x02: 	// Config01_Read
						if(sid.sid_model == MOS_8580)
							val |= 0x01;
						if(sid.filter_enable)
							val |= 0x02;
						if(sid.extfilter_enable)
							val |= 0x04;
						if(sid.digi_boost_enable)
							val |= 0x08;
						sid_io[0x1d] = val;
						break;

					case 0x03: // LED On
						gpio_put(PICO_LED_PIN, true);
						is_ready = false;
						break;

					case 0x04: // LED Off
						gpio_put(PICO_LED_PIN, false);
						is_ready = false;
						break;

					case 0xfd:
						sid_io[0x1d] = VERSION_MAJOR;
						is_ready = false;
						break;

					case 0xfe:
						sid_io[0x1d] = VERSION_MINOR;
						is_ready = false;
						break;

					case 0xff:
						sid_io[0x1d] = VERSION_PATCH;
						is_ready = false;
						break;

					default:
						is_ready = false;
						break;
					}
				}
				else
				{
					// execude command with value
					switch (last_command)
					{
					case 0x00:	// XOR von Value zurückgeben
						sid_io[0x1d] = value ^ 0x88;
						break;
					case 0x01:	// Configuration lesen

						configuration[CONFIG_01] = value;	// Config_1
						config_is_new = true;

						if(value & 0x01)
							sid.SetSidType(MOS_8580);
						else	
						sid.SetSidType(MOS_6581);
						sid.EnableFilter(value & 0x02);
						sid.EnableExtFilter(value & 0x04);
						sid.EnableDigiBoost8580(value & 0x08);

						ConfigOutput();

						break;
					}
						is_command = false;
						is_ready = false;
				}
			}
		break;

		case 0x1e:
			if(is_ready)
			{
				is_ready = false;
			}
		break;
		
		case 0x1f:
			if(is_ready)
			{
				is_ready = false;
			}
		break;
	}
}

void WriteSidReg()
{
	if (pio0_hw->irq & 1) 
	{
		pio0_hw->irq = 1;

		uint16_t incomming = pio->rxf[sm0];
		uint8_t sid_reg = (incomming >> 2) & 0x1f;
		uint8_t sid_value = (incomming >> 7) & 0xff;

		sid.WriteReg(sid_reg, sid_value);
		CheckConfig(sid_reg, sid_value);
	}
}

void Core1Entry() 
{
	multicore_lockout_victim_init(); 	// wird benötigt um den core1 anhalten zu können von Core 0

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
	vreg_set_voltage( VREG_VOLTAGE_1_30);
	sleep_ms(100);
	set_sys_clock_khz(SYSTEM_CLOCK, true);

	stdio_init_all();	

	// Pico LED
	gpio_init(PICO_LED_PIN);
	gpio_set_dir(PICO_LED_PIN, true);

	// ADC
	gpio_init(ADC_2KHz_PIN);
	gpio_set_dir(ADC_2KHz_PIN, true);	// Output

	gpio_init(ADC0_COMPARE_PIN);
	gpio_init(ADC1_COMPARE_PIN);

	gpio_set_dir(ADC0_COMPARE_PIN, false);	// Input
	gpio_set_dir(ADC1_COMPARE_PIN, false);	// Input

	// Init SID
	// memory for the sid i

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

	for(int i=0; i<32; i++)
	{
		sid.WriteReg(i, 0);
		sid_io[i] = 0;
	}

	// ReadConfig
	ReadConfig();

	// Output Coniguration to Serial
	ConfigOutput();

	gpio_put(PICO_LED_PIN, true);

    while (1)
    {
		if(adc_enable)
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

		if(config_is_new)
		{
			config_is_new = false;
			WriteConfig();
		}
    }
}

void pwm_irq_handle()
{
	pwm_clear_irq(slice_num);

		for(int i=0; i<3; i++) 	// 279MHz // with extfilter enabled
		{
			sid.Clock(8);			
			
			sid_io[0x1b] = sid.voice[2].wave.ReadOSC();
			sid_io[0x1c] = sid.voice[2].envelope.ReadEnv();
		}

	/*
	if(sid.extfilter_enable)
	{
		for(int i=0; i<4; i++) 	// 300MHz // with extfilter enabled
		{
			sid.Clock(6);			
			sid_io[0x1b] = sid.voice[2].wave.ReadOSC();
			sid_io[0x1c] = sid.voice[2].envelope.ReadEnv();
		}
	}
	else
	{
		for(int i=0; i<6; i++) 	// 300MHz // with extfilter disabled
		{
			sid.Clock(4);			
			sid_io[0x1b] = sid.voice[2].wave.ReadOSC();
			sid_io[0x1c] = sid.voice[2].envelope.ReadEnv();
		}
	}
	*/
	
	uint16_t out = sid.AudioOut(11) + 1024;

	pwm_set_gpio_level(AUDIO_PIN, out);
}

void InitPWMAudio(uint audio_out_gpio)
{
	gpio_set_function(audio_out_gpio, GPIO_FUNC_PWM);
	gpio_set_drive_strength(audio_out_gpio, GPIO_DRIVE_STRENGTH_2MA);

	// Find out which PWM slice is connected to GPIO 2 (it's slice 1)
	slice_num = pwm_gpio_to_slice_num(audio_out_gpio);

	// Set pwm frequenz
	pwm_set_clkdiv_int_frac(slice_num, 3, 5);	// PWM Frequency of 41126Hz when Systemclock is 279MHz.	// PAL
	//pwm_set_clkdiv_int_frac(slice_num, 3, 3);	// PWM Frequency of 42738Hz when Systemclock is 279MHz.	// NTSC

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
    uint write_channel =1;
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