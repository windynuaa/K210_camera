#include "ov2460.h"
#include "stdio.h"
#include "dvp.h"
#include "lcd.h"
#include "plic.h"
#include "fpioa.h"
#include "stdlib.h"
#include "sysctl.h"
#include "sleep.h"
int g_dvp_finish_flag=0;
uint16_t k210_dataBuffer[76800];
uint8_t k210_aiBuffer[240*320*3];
uint16_t* get_k210_aiBuffer()
{
	return k210_aiBuffer;
}
uint16_t* get_k210_dataBuffer()
{
	return k210_dataBuffer;
}
uint64_t millis() {
	return sysctl_get_time_us() / 1000;
}

int cambus_read_id(uint8_t addr, uint16_t* manuf_id, uint16_t* device_id)
{
	dvp_sccb_send_data(addr, 0xFF, 0x01);
	*manuf_id = (dvp_sccb_receive_data(addr, 0x1C) << 8) | dvp_sccb_receive_data(addr, 0x1D);
	*device_id = (dvp_sccb_receive_data(addr, 0x0A) << 8) | dvp_sccb_receive_data(addr, 0x0B);
	return 0;
}

int cambus_readb(uint8_t slv_addr, uint8_t reg_addr, uint8_t* reg_data)
{

	int ret = 0;
	*reg_data = dvp_sccb_receive_data(slv_addr, reg_addr);

	if (0xff == *reg_data)
		ret = -1;

	return ret;

}

int cambus_writeb(uint8_t slv_addr, uint8_t reg_addr, uint8_t reg_data)
{
	dvp_sccb_send_data(slv_addr, reg_addr, reg_data);
	return 0;
}

int cambus_scan()
{
	uint16_t manuf_id = 0;
	uint16_t device_id = 0;
	for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
		cambus_read_id(addr, &manuf_id, &device_id);
		if (0xffff != device_id)
		{
			return addr;
		}
	}
	return 0;
}

static int sensor_irq(void* ctx)
{
	if (dvp_get_interrupt(DVP_STS_FRAME_FINISH)) {	//frame end
		dvp_clear_interrupt(DVP_STS_FRAME_FINISH);
		g_dvp_finish_flag = 1;
		//lcd_draw_picture(0, 0, 320, 240, (uint16_t*)k210_dataBuffer);
		//printf("finsifhsadjsadj");
	}
	else {	//frame start
		if (g_dvp_finish_flag == 0)  //only we finish the convert, do transmit again
			dvp_start_convert();	//so we need deal img ontime, or skip one framebefore next
		dvp_clear_interrupt(DVP_STS_FRAME_START);
	}

	return 0;
}

int dvpInitIrq()
{
	dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 0);
	plic_set_priority(IRQN_DVP_INTERRUPT,10);
	/* set irq handle */
	plic_irq_register(IRQN_DVP_INTERRUPT, sensor_irq, (void*)NULL);

	plic_irq_disable(IRQN_DVP_INTERRUPT);
	dvp_clear_interrupt(DVP_STS_FRAME_START | DVP_STS_FRAME_FINISH);
	dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 1);

	return 0;
}

int dvpInit(uint32_t freq)
{

	fpioa_set_function(47, FUNC_CMOS_PCLK);
	fpioa_set_function(46, FUNC_CMOS_XCLK);
	fpioa_set_function(45, FUNC_CMOS_HREF);
	fpioa_set_function(44, FUNC_CMOS_PWDN);
	fpioa_set_function(43, FUNC_CMOS_VSYNC);
	fpioa_set_function(42, FUNC_CMOS_RST);
	fpioa_set_function(41, FUNC_SCCB_SCLK);
	fpioa_set_function(40, FUNC_SCCB_SDA);

	/* Do a power cycle */
	DCMI_PWDN_HIGH();
	msleep(10);

	DCMI_PWDN_LOW();
	msleep(10);

	// Initialize the camera bus, 8bit reg
	dvp_init(8);
	// Initialize dvp interface
	dvp_set_xclk_rate(freq);
	dvp->cmos_cfg |= DVP_CMOS_CLK_DIV(3) | DVP_CMOS_CLK_ENABLE;
	dvp_enable_burst();
	dvp_disable_auto();
	dvp_set_output_enable(DVP_OUTPUT_AI, 1);	//enable to AI
	dvp_set_output_enable(DVP_OUTPUT_DISPLAY, 1);	//enable to lcd
	dvp_set_image_format(DVP_CFG_RGB_FORMAT);
	dvp_set_image_size(CAM_WIDTH, CAM_HEIGHT);	//set QVGA default
	dvp_set_ai_addr((uint32_t)((long)k210_aiBuffer), (uint32_t)((long)(k210_aiBuffer + CAM_WIDTH * CAM_HEIGHT)), (uint32_t)((long)(k210_aiBuffer + CAM_WIDTH * CAM_HEIGHT * 2)));
	dvp_set_display_addr((uint32_t)((long)k210_dataBuffer));
	return 0;
}

int OV2640_reset(int addr)
{
	int i = 0;
	const uint8_t(*regs)[2];

	/* Reset all registers */
	cambus_writeb(addr, BANK_SEL, BANK_SEL_SENSOR);
	cambus_writeb(addr, COM7, COM7_SRST);

	/* delay n ms */
	msleep(10);

	i = 0;
	regs = ov2640_config;//default_regs,ov2640_default
	/* Write initial regsiters */
	while (regs[i][0]) {
		cambus_writeb(addr, regs[i][0], regs[i][1]);
		i++;
	}
	i = 0;
	return 0;
}

int OV2640_read_reg(int addr,uint8_t reg_addr)
{
	uint8_t reg_data;
	if (cambus_readb(addr, reg_addr, &reg_data) != 0) {
		return -1;
	}
	return reg_data;
}

int OV2640_write_reg(int addr,uint8_t reg_addr, uint8_t reg_data)
{
	return cambus_writeb(addr, reg_addr, reg_data);
}



int Sipeed_OV2640_reverse_u32pixel(uint32_t* addr, uint32_t length)
{
	if (NULL == addr)
		return -1;

	uint32_t data;
	uint32_t* pend = addr + length;
	for (; addr < pend; addr++)
	{
		data = *(addr);
		*(addr) = ((data & 0x000000FF) << 24) | ((data & 0x0000FF00) << 8) |
			((data & 0x00FF0000) >> 8) | ((data & 0xFF000000) >> 24);
	}  //1.7ms


	return 0;
}

int Sipeed_OV2640_sensor_snapshot()
{
	unsigned int i;
	//wait for new frame
	g_dvp_finish_flag = 0;
	//uint32_t start = millis();
	i = 0;
	while (g_dvp_finish_flag == 0)
	{
		i++;
		if (i>400000000)//wait for 300ms
			return -1;
	}

	Sipeed_OV2640_reverse_u32pixel((uint32_t*)k210_dataBuffer, CAM_WIDTH * CAM_HEIGHT / 2);

	return 0;
}

int Sipeed_OV2640_run(int run)
{
	if (run)
	{
		dvp_clear_interrupt(DVP_STS_FRAME_START | DVP_STS_FRAME_FINISH);
		plic_irq_enable(IRQN_DVP_INTERRUPT);
		dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 1);
	}
	else {
		plic_irq_disable(IRQN_DVP_INTERRUPT);
		dvp_clear_interrupt(DVP_STS_FRAME_START | DVP_STS_FRAME_FINISH);
		dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 0);
	}
	return 1;
}

int Sipeed_OV2640_begin(int addr,int freq)
{

	if (!Sipeed_OV2640_reset(addr,freq))
		return 0;
	return 1;
}


int Sipeed_OV2640_reset(int addr,int freq)
{
	if (dvpInit(freq) != 0)
		return 0;
	if (OV2640_reset(addr) != 0)
		return 0;
	if (dvpInitIrq() != 0)
		return 0;
	return 1;
}