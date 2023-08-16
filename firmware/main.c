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
#include <hardware/irq.h>
#include <hardware/pwm.h>
#include <hardware/sync.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <hardware/clocks.h>
#include <pico/binary_info.h>

#include "write_sid_reg.pio.h"

#include "sid.h"

#define RES_PIN 2
#define CS_PIN 3
#define CLK_PIN 4
#define RW_PIN 5

#define AUDIO_PIN 28

#define DEBUG_LED_PIN 19

volatile static bool reset_state = true;

bool AudioCallback();
void InitPWMAudio(uint audio_out_gpio);

float *sound_buffer;

SID sid1;

int main() {
    bi_decl(bi_program_description("The Pico SID"));
    bi_decl(bi_1pin_with_name(PICO_DEFAULT_LED_PIN, "On-board LED"));
    bi_decl(bi_1pin_with_name(RES_PIN, "C64 Reset"));
    bi_decl(bi_1pin_with_name(CLK_PIN, "C64 Clock"));
    bi_decl(bi_1pin_with_name(RW_PIN, "C64 SID RW"));
    bi_decl(bi_1pin_with_name(CS_PIN, "C64 SID Chip Select"));

	set_sys_clock_khz(176000, true);

	stdio_init_all();	

	// Init Debug LED (green/red)
	gpio_set_drive_strength(DEBUG_LED_PIN, GPIO_DRIVE_STRENGTH_12MA);
	gpio_set_function(DEBUG_LED_PIN, GPIO_FUNC_SIO);
	gpio_set_dir(DEBUG_LED_PIN, true);
	gpio_put(DEBUG_LED_PIN, true);

	// PIO Program initialize
	PIO pio = pio0;

	uint offset = pio_add_program(pio, &write_sid_reg_program);
	uint sm = pio_claim_unused_sm(pio, true);

	write_sid_reg_program_init(pio, sm, offset, CLK_PIN, CS_PIN);	//CLK_PIN + RW_PIN + A0-A4 + D0-D7 all PIN Count is 15

	// Init SID
	SidInit(&sid1);

	// Init Audio PWM
	InitPWMAudio(AUDIO_PIN);
	
	// UART Start Message PicoSID
	printf("The Pico SID by Thorsten Kattanek 1\n");

    while (1)
    {
		while (!(pio->irq & (1<<0))); 		// wait for irq 1 fro either of the pio programs
    		pio_interrupt_clear(pio, 0); 	// clear the irq so we can wait for it again
		
		uint16_t value = pio->rxf[sm];
		SidWriteReg(&sid1, value >> 2, (value >> 7) & 0xff);
    }
}

void PWMMInterruptHandler() 
{
    pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_PIN));   

#ifdef SID_CYCLE_EXACT

	for(int i=0; i<22; i++)
	{
		OscOneCycle(&sid1.Osc[0]);
		OscOneCycle(&sid1.Osc[1]);
		OscOneCycle(&sid1.Osc[2]);

		EnvOneCycle(&sid1.Env[0]);
		EnvOneCycle(&sid1.Env[1]);
		EnvOneCycle(&sid1.Env[2]);
	}

#else

for(int i=0; i<2; i++)
{
	OscExecuteCycles(&sid1.Osc[0], 11);
	EnvExecuteCycles(&sid1.Env[0], 11);
}

#endif

	// 10 Bit Sample schreiben	
	float out_sample1 = OscGetOutput(&sid1.Osc[0]) / (float)0xfff;
	float out_sample2 = OscGetOutput(&sid1.Osc[1]) / (float)0xfff;
	float out_sample3 = OscGetOutput(&sid1.Osc[2]) / (float)0xfff;

	out_sample1 *= EnvGetOutput(&sid1.Env[0]) / (float)0xff;
	out_sample2 *= EnvGetOutput(&sid1.Env[1]) / (float)0xff;
	out_sample3 *= EnvGetOutput(&sid1.Env[2]) / (float)0xff;

	float out_sample = ((out_sample1 + out_sample2 + out_sample3) / 3.0f) * 0x7ff * sid1.VolumeOut;
	//float out_sample = ((out_sample1 + out_sample2)/ 2.0f) * 0x7ff * sid1.VolumeOut;

	pwm_set_gpio_level(AUDIO_PIN, out_sample);
}

void InitPWMAudio(uint audio_out_gpio)
{
	gpio_set_function(audio_out_gpio, GPIO_FUNC_PWM);
	int audio_pin_slice = pwm_gpio_to_slice_num(audio_out_gpio);

	// Setup PWM interrupt to fire when PWM cycle is complete
    pwm_clear_irq(audio_pin_slice);
    pwm_set_irq_enabled(audio_pin_slice, true);
    // set the handle function above
    irq_set_exclusive_handler(PWM_IRQ_WRAP, PWMMInterruptHandler); 
    irq_set_enabled(PWM_IRQ_WRAP, true);

    // Setup PWM for audio output
    pwm_config config = pwm_get_default_config();
    /* Base clock 176,000,000 Hz divide by wrap 1023 then the clock divider further divides
     * So clkdiv should be as follows for given sample rate
     *  15.6f for 11,089 KHz
     *  7.8f  for 22,177 KHz
     *  3.9f  for 44,355 KHz
     */
	pwm_config_set_clkdiv_mode(&config, PWM_DIV_FREE_RUNNING);
    pwm_config_set_clkdiv(&config, 1.98f); 
    pwm_config_set_wrap(&config, 2047); 		// 11 Bit
    pwm_init(audio_pin_slice, &config, true);

    pwm_set_gpio_level(audio_out_gpio, 0);
}