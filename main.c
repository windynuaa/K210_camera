#include "lcd.h"
#include "dvp.h"
#include "plic.h"
#include "gpio.h"
#include "gpiohs.h"
#include "plic.h"
#include "dmac.h"
#include "fpioa.h"
#include <bsp.h>
#include <sysctl.h>
#include "uarths.h"
#include <stdio.h>
#include "ov2460.h"


//uint8_t cam_buf[320 * 240 * 2] __attribute__((aligned(64)));
//uint8_t lcd_buf[320 * 240 * 2] __attribute__((aligned(64)));

void pll_init()
{
	sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL);
	sysctl_pll_set_freq(SYSCTL_PLL1, 300000000UL);
	sysctl_pll_set_freq(SYSCTL_PLL2, 45158400UL);
}

void power_init()
{
	sysctl_set_power_mode(SYSCTL_POWER_BANK6, SYSCTL_POWER_V18);
	sysctl_set_power_mode(SYSCTL_POWER_BANK7, SYSCTL_POWER_V18);
}
void system_init()
{
	pll_init();
	power_init();
	plic_init();
	sysctl_enable_irq();
	uarths_init();
	gpio_init();
	//io_mux_init();
	sysctl_set_spi0_dvp_data(1);
	dmac_init();
	dvpInit(12000000);
}


int main()
{
	int a;
	uint16_t* framebudd;
	system_init();
	lcd_init(0, 3, 6, 7, 15000000, 37,  38, 3);
	lcd_clear(0xFFFF);
	a = cambus_scan();
	Sipeed_OV2640_begin(a, 24000000);
	//Sipeed_OV2640_run(1);
	framebudd = get_k210_dataBuffer();

	while (1)
	{
		//lcd_fill_rectangle(0, 0, 320, 240, RED);
		//msleep(2000);
		dvp_start_convert();
		Sipeed_OV2640_sensor_snapshot();
		//dvp_finish_convert();
		//lcd_fill_rectangle(0, 0, 160, 120, RED);
		//msleep(1000);
		//dvp_finish_convert();
		//Sipeed_OV2640_run(0);
		lcd_draw_picture(0, 0, 320, 240, (uint16_t*)framebudd);
		//Sipeed_OV2640_run(1);
		//dvp_start_convert();
		//msleep(30);
	}
	
	//	lcd_draw_picture(0, 0, 320, 240, k210_dataBuffer);
}