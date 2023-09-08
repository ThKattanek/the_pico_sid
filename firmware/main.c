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
#include <pico/multicore.h>
#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/pwm.h>

#include "write_sid_reg.pio.h"

#include "sid.h"

#define RES_PIN 2
#define CS_PIN 3
#define CLK_PIN 4	
#define CLK_2_PIN 20	// 1/2 CLK from CLK_PIN
#define RW_PIN 5

#define AUDIO_PIN 28

#define DEBUG_LED_PIN 19

volatile static bool reset_state = true;

PIO pio;
uint sm;

uint slice_num;

static SID sid1;

void InitPWMAudio(uint audio_out_gpio);

void WriteSidReg()
{
	uint16_t incomming;

	if (pio0_hw->irq & 1) 
	{
    	pio0_hw->irq = 1;

		incomming = pio->rxf[sm];
		SidWriteReg(&sid1, incomming >> 2, (incomming >> 7) & 0xff);
	}
}

bool led_state = true;

void Core1Entry() 
{
    while (1)
	{
		while(!gpio_get(CLK_2_PIN)){}

		// One Cycle SID
		led_state = !led_state;
		gpio_put(DEBUG_LED_PIN, led_state);

		OscExecuteCycles(&sid1.Osc, 4);
		EnvExecuteCycles(&sid1.Env, 4);
		

		while(gpio_get(CLK_2_PIN)){}

		
	}
}

int main() {

	//default clock is 125000kHz
	set_sys_clock_khz(170000, true); // Kann Normal bis 133MHz laufen, funktioniert bei mir auch bis 266MHz

	stdio_init_all();	

	// Init Debug LED (green/red)
	gpio_set_drive_strength(DEBUG_LED_PIN, GPIO_DRIVE_STRENGTH_2MA);
	gpio_set_function(DEBUG_LED_PIN, GPIO_FUNC_SIO);
	gpio_set_dir(DEBUG_LED_PIN, true);
	gpio_put(DEBUG_LED_PIN, led_state);

	// CLK 1/2
	gpio_set_function(CLK_2_PIN, GPIO_FUNC_SIO);
	gpio_set_dir(CLK_2_PIN, false);

	// Init SID
	SidInit(&sid1);

	// PIO Program initialize
	pio = pio0;

	uint offset = pio_add_program(pio, &write_sid_reg_program);
	sm = pio_claim_unused_sm(pio, true);

	write_sid_reg_program_init(pio, sm, offset, CLK_PIN, CS_PIN);	//CLK_PIN + RW_PIN + A0-A4 + D0-D7 all PIN Count is 15

	irq_set_exclusive_handler(PIO0_IRQ_0, WriteSidReg);
	irq_set_enabled(PIO0_IRQ_0, true);
	
	pio0_hw->inte0 = PIO_IRQ0_INTE_SM0_BITS | PIO_IRQ0_INTE_SM1_BITS;

	// Init Audio PWM
	InitPWMAudio(AUDIO_PIN);
	
	// UART Start Message PicoSID
	printf("The Pico SID by Thorsten Kattanek 1\n");

	// Start Core#1 for SID Emualtion
	multicore_launch_core1(Core1Entry);

    while (1)
    {
    }
}

/*
void FillAudioBuffer(void)
{	
	uint16_t *sound_buffer;

	bool fill_audio = false;

	if (dma_hw->ints1 & (1 << dma_chan_1)) // are we called for our DMA channel 1? 
	{
		dma_hw->ints1 = (1 << dma_chan_1); // clear IRQ DMA Channel 1
		sound_buffer = sound_buffer1;
		fill_audio = true;
	}
	else if(dma_hw->ints1 & (1 << dma_chan_2))
	{
		dma_hw->ints1 = (1 << dma_chan_2); // clear IRQ DMA Channel 2
		sound_buffer = sound_buffer2;
		fill_audio = true;
	}

	// filling sound_buffer
	if(fill_audio)
	{
		for(int i=0; i<SOUND_BUFFER_SIZE; i++)
		{
			#ifdef SID_CYCLE_EXACT
				OscMoreCycles(sid1.Osc, 22);
				// EnvMoreCycles(sid1.Env, 22);			
			#else
				OscExecuteCycles(&sid1.Osc[0], 22);
				EnvExecuteCycles(&sid1.Env[0], 22);
			#endif

			// 11 Bit Sample schreiben	
			float out_sample1 = OscGetOutput(&sid1.Osc[0]) / (float)0xfff;
			float out_sample2 = OscGetOutput(&sid1.Osc[1]) / (float)0xfff;
			float out_sample3 = OscGetOutput(&sid1.Osc[2]) / (float)0xfff;

			//out_sample1 *= EnvGetOutput(&sid1.Env[0]) / (float)0xff;
			//out_sample2 *= EnvGetOutput(&sid1.Env[1]) / (float)0xff;
			//out_sample3 *= EnvGetOutput(&sid1.Env[2]) / (float)0xff;

			sound_buffer[i] = ((out_sample1 + out_sample2 + out_sample3) / 3.0f) * 0x7ff * sid1.VolumeOut;
			
			// sound_buffer[i] = out_sample1 * 0x7ff * sid1.VolumeOut;
		}
	}
}
*/

void pwm_irq_handle()
{
	pwm_clear_irq(slice_num);

	float out_sample1 = OscGetOutput(&sid1.Osc[0]) / (float)0xfff;
	float out_sample2 = OscGetOutput(&sid1.Osc[1]) / (float)0xfff;
	float out_sample3 = OscGetOutput(&sid1.Osc[2]) / (float)0xfff;

	out_sample1 *= EnvGetOutput(&sid1.Env[0]) / (float)0xff;
	out_sample2 *= EnvGetOutput(&sid1.Env[1]) / (float)0xff;
	out_sample3 *= EnvGetOutput(&sid1.Env[2]) / (float)0xff;
		
	uint16_t sample_out = ((out_sample1 + out_sample2 + out_sample3) / 3.0f) * 0x7ff * sid1.VolumeOut;

	pwm_set_gpio_level(AUDIO_PIN, sample_out);
}

void InitPWMAudio(uint audio_out_gpio)
{
	gpio_set_function(audio_out_gpio, GPIO_FUNC_PWM);
	gpio_set_drive_strength(audio_out_gpio, GPIO_DRIVE_STRENGTH_2MA);

	// Find out which PWM slice is connected to GPIO 2 (it's slice 1)
	slice_num = pwm_gpio_to_slice_num(audio_out_gpio);

	// Set pwm frequenz
	pwm_set_clkdiv_int_frac(slice_num, 1,14);	// PWM Frequency of 43294Hz when Systemclock is 133MHz.

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