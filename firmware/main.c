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

// PIN MAPPING
#define RES_PIN 2
#define CS_PIN 3
#define CLK_PIN 4	
#define CLK_2_PIN 20	// 1/2 CLK from CLK_PIN
#define RW_PIN 5
#define AUDIO_PIN 28
#define DEBUG_LED_PIN 19

volatile bool reset_state = true;

volatile PIO pio;
volatile uint sm;

volatile uint slice_num;

uint16_t samples1[32] __attribute__((aligned(64)));
uint16_t samples2[32] __attribute__((aligned(64)));

int dma_chan_1;
int dma_chan_2;

//#define RINGBUFFER_ENABLE

int sample;

#ifdef RINGBUFFER_ENABLE
volatile uint16_t register_ring_buffer[0x1000];
volatile uint16_t register_ring_buffer_rpos = 0;
volatile uint16_t register_ring_buffer_wpos = 0;
#endif

bool led_state = true;

void InitPWMAudio(uint audio_out_gpio);

void WriteSidReg()
{
	if (pio0_hw->irq & 1) 
	{
		pio0_hw->irq = 1;

#ifdef RINGBUFFER_ENABLE
		uint16_t incomming = pio->rxf[sm];
		register_ring_buffer[register_ring_buffer_wpos & 0xfff] = pio->rxf[sm];	
		register_ring_buffer_wpos++;
#else
		uint16_t incomming = pio->rxf[sm];
		SidWriteReg(incomming >> 2, (incomming >> 7) & 0xff);
#endif
	}
}

void Core1Entry() 
{
	InitPWMAudio(AUDIO_PIN);

    while (1)
	{
	}
}

int main() {

	//default clock is 125000kHz
	set_sys_clock_khz(248000, true); // Kann Normal bis 133MHz laufen, funktioniert bei mir auch bis 266MHz

	stdio_init_all();	

	// Init Debug LED (green/red)
	gpio_set_drive_strength(DEBUG_LED_PIN, GPIO_DRIVE_STRENGTH_2MA);
	gpio_set_function(DEBUG_LED_PIN, GPIO_FUNC_SIO);
	gpio_set_dir(DEBUG_LED_PIN, true);
	gpio_put(DEBUG_LED_PIN, led_state);

	// CLK 1/4
	gpio_set_function(CLK_2_PIN, GPIO_FUNC_SIO);
	gpio_set_dir(CLK_2_PIN, false);

	// Init SID
	SidInit();
	SidSetChipTyp(MOS_8580);
	SidEnableFilter(true);

	// Init Audio PWM
	//InitPWMAudio(AUDIO_PIN);
	
	// UART Start Message PicoSID
	printf("The Pico SID by Thorsten Kattanek\n");

	// Start Core#1 for SID Emualtion
	multicore_launch_core1(Core1Entry);
	
	// PIO Program initialize
	pio = pio0;

	uint offset = pio_add_program(pio, &write_sid_reg_program);
	sm = pio_claim_unused_sm(pio, true);

	write_sid_reg_program_init(pio, sm, offset, CLK_PIN, CS_PIN);	//CLK_PIN + RW_PIN + A0-A4 + D0-D7 all PIN Count is 15

	irq_set_exclusive_handler(PIO0_IRQ_0, WriteSidReg);
	irq_set_enabled(PIO0_IRQ_0, true);

	pio0_hw->inte0 = PIO_IRQ0_INTE_SM0_BITS | PIO_IRQ0_INTE_SM1_BITS;

    while (1)
    {
#ifdef RINGBUFFER_ENABLE	
		if(register_ring_buffer_rpos < register_ring_buffer_wpos)
		{
			uint16_t incomming = register_ring_buffer[register_ring_buffer_rpos & 0xfff];
			SidWriteReg(incomming >> 2, (incomming >> 7) & 0xff);
			register_ring_buffer_rpos++;
		}else if(register_ring_buffer_rpos > register_ring_buffer_wpos)
		{
			/// buffer overflow
			register_ring_buffer_rpos = register_ring_buffer_wpos;
			printf("Buffer Overflow\n");
		}
#endif
    }
}

void pwm_irq_handle()
{
	pwm_clear_irq(slice_num);

	led_state = !led_state;
	gpio_put(DEBUG_LED_PIN, led_state);

	for(int i=0; i<6; i++)
	{
		SidCycle(4);
	}

	uint16_t out = ((SidFilterOut() >> 4) + 32768) / (float)0xffff * 0x7ff;
	
	pwm_set_gpio_level(AUDIO_PIN, out);
}

void InitPWMAudio(uint audio_out_gpio)
{
	gpio_set_function(audio_out_gpio, GPIO_FUNC_PWM);
	gpio_set_drive_strength(audio_out_gpio, GPIO_DRIVE_STRENGTH_2MA);

	// Find out which PWM slice is connected to GPIO 2 (it's slice 1)
	slice_num = pwm_gpio_to_slice_num(audio_out_gpio);

	// Set pwm frequenz
	pwm_set_clkdiv_int_frac(slice_num, 2,15);	// PWM Frequency of 41057,2Hz when Systemclock is 247MHz.

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