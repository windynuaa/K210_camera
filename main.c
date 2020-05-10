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
#include "ov2640.h"

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
	sysctl_set_spi0_dvp_data(1);
	dmac_init();
	dvpInit(12000000);//for ov2640 addr detecting 用于扫描0V2640硬件地址
}


int main()
{
	int a;
	uint16_t* framebuffer;
	system_init();
	lcd_init(0, 3, 6, 7, 15000000, 37,  38, 3);//320w 240h
	lcd_clear(0xFFFF);
	a = cambus_scan();//GET OV2640 ADDR 得到2640硬件地址
	Sipeed_OV2640_begin(a, 24000000);
	Sipeed_OV2640_run(1);
	framebuffer = get_k210_dataBuffer();//get display_data_buffer address 得到显示数据地址

	while (1)
	{
		Sipeed_OV2640_sensor_snapshot();
		lcd_draw_picture(0, 0, 320, 240, (uint16_t*)framebuffer);
	}
}
