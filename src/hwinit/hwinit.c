/*
* Copyright (c) 2018 naehrwert
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
*
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "clock.h"
#include "uart.h"
#include "i2c.h"
#include "sdram.h"
#include "di.h"
#include "mc.h"
#include "t210.h"
#include "pmc.h"
#include "gpio.h"
#include "pinmux.h"
#include "max77620.h"
#include "fuse.h"
#include "util.h"

void config_oscillators()
{
	CLOCK(CLK_RST_CONTROLLER_SPARE_REG0) = (CLOCK(CLK_RST_CONTROLLER_SPARE_REG0) & 0xFFFFFFF3) | 4;
	SYSCTR0(SYSCTR0_CNTFID0) = 19200000;
	TMR(0x14) = 0x45F;
	CLOCK(CLK_RST_CONTROLLER_OSC_CTRL) = 0x50000071;
	PMC(APBDEV_PMC_OSC_EDPD_OVER) = (PMC(APBDEV_PMC_OSC_EDPD_OVER) & 0xFFFFFF81) | 0xE;
	PMC(APBDEV_PMC_OSC_EDPD_OVER) = (PMC(APBDEV_PMC_OSC_EDPD_OVER) & 0xFFBFFFFF) | 0x400000;
	PMC(APBDEV_PMC_CNTRL2) = (PMC(APBDEV_PMC_CNTRL2) & 0xFFFFEFFF) | 0x1000;
	PMC(APBDEV_PMC_SCRATCH188) = (PMC(APBDEV_PMC_SCRATCH188) & 0xFCFFFFFF) | 0x2000000;
	CLOCK(CLK_RST_CONTROLLER_CLK_SYSTEM_RATE) = 0x10;
	CLOCK(CLK_RST_CONTROLLER_PLLMB_BASE) &= 0xBFFFFFFF;
	PMC(APBDEV_PMC_TSC_MULT) = (PMC(APBDEV_PMC_TSC_MULT) & 0xFFFF0000) | 0x249F; //0x249F = 19200000 * (16 / 32.768 kHz)
	CLOCK(CLK_RST_CONTROLLER_SCLK_BURST_POLICY) = 0x20004444;
	CLOCK(CLK_RST_CONTROLLER_SUPER_SCLK_DIVIDER) = 0x80000000;
	CLOCK(CLK_RST_CONTROLLER_CLK_SYSTEM_RATE) = 2;
}

void config_gpios()
{
	pinmux_set_config(PINMUX_UART2_TX_INDEX, 0);
	pinmux_set_config(PINMUX_UART3_TX_INDEX, 0);
	pinmux_set_config(PINMUX_GPIO_PE6_INDEX, PINMUX_INPUT_ENABLE);
	pinmux_set_config(PINMUX_GPIO_PH6_INDEX, PINMUX_INPUT_ENABLE);

	gpio_config(GPIO_BY_NAME(UART2_TX), GPIO_MODE_GPIO);
	gpio_config(GPIO_BY_NAME(UART3_TX), GPIO_MODE_GPIO);
	gpio_config(GPIO_DECOMPOSE(GPIO_E6_INDEX), GPIO_MODE_GPIO);
	gpio_config(GPIO_DECOMPOSE(GPIO_H6_INDEX), GPIO_MODE_GPIO);
	gpio_output_enable(GPIO_BY_NAME(UART2_TX), GPIO_OUTPUT_DISABLE);
	gpio_output_enable(GPIO_BY_NAME(UART3_TX), GPIO_OUTPUT_DISABLE);
	gpio_output_enable(GPIO_DECOMPOSE(GPIO_E6_INDEX), GPIO_OUTPUT_DISABLE);
	gpio_output_enable(GPIO_DECOMPOSE(GPIO_H6_INDEX), GPIO_OUTPUT_DISABLE);

	pinmux_config_i2c(I2C_1);
	pinmux_config_i2c(I2C_PWR);

	//unused UART_A
	PINMUX_SET_UNUSED_BY_NAME(UART1_RX);
	PINMUX_SET_UNUSED_BY_NAME(UART1_TX);
	PINMUX_SET_UNUSED_BY_NAME(UART1_CTS);
	PINMUX_SET_UNUSED_BY_NAME(UART1_RTS);

	//Configure volume up/down as inputs.
	gpio_config(GPIO_BY_NAME(BUTTON_VOL_UP), GPIO_MODE_GPIO);
	gpio_config(GPIO_BY_NAME(BUTTON_VOL_DOWN), GPIO_MODE_GPIO);
	gpio_output_enable(GPIO_BY_NAME(BUTTON_VOL_UP), GPIO_OUTPUT_DISABLE);
	gpio_output_enable(GPIO_BY_NAME(BUTTON_VOL_DOWN), GPIO_OUTPUT_DISABLE);
}

void config_pmc_scratch()
{
	PMC(APBDEV_PMC_SCRATCH20) &= 0xFFF3FFFF;
	PMC(APBDEV_PMC_SCRATCH190) &= 0xFFFFFFFE;
	PMC(APBDEV_PMC_SECURE_SCRATCH21) |= 0x10;
}

void mbist_workaround()
{
	CLOCK(0x410) = (CLOCK(0x410) | 0x8000) & 0xFFFFBFFF;
	CLOCK(0xD0) |= 0x40800000u;
	CLOCK(0x2AC) = 0x40;
	CLOCK(0x294) = 0x40000;
	CLOCK(0x304) = 0x18000000;
	sleep(2);

	I2S(0x0A0) |= 0x400;
	I2S(0x088) &= 0xFFFFFFFE;
	I2S(0x1A0) |= 0x400;
	I2S(0x188) &= 0xFFFFFFFE;
	I2S(0x2A0) |= 0x400;
	I2S(0x288) &= 0xFFFFFFFE;
	I2S(0x3A0) |= 0x400;
	I2S(0x388) &= 0xFFFFFFFE;
	I2S(0x4A0) |= 0x400;
	I2S(0x488) &= 0xFFFFFFFE;
	DISPLAY_A(0xCF8) |= 4;
	VIC(0x8C) = 0xFFFFFFFF;
	sleep(2);

	CLOCK(0x2A8) = 0x40;
	CLOCK(0x300) = 0x18000000;
	CLOCK(0x290) = 0x40000;
	CLOCK(CLK_RST_CONTROLLER_CLK_OUT_ENB_H) = 0xC0;
	CLOCK(CLK_RST_CONTROLLER_CLK_OUT_ENB_L) = 0x80400130;
	CLOCK(CLK_RST_CONTROLLER_CLK_OUT_ENB_U) = 0x1F00200;
	CLOCK(CLK_RST_CONTROLLER_CLK_OUT_ENB_V) = 0x80400808;
	CLOCK(CLK_RST_CONTROLLER_CLK_OUT_ENB_W) = 0x402000FC;
	CLOCK(CLK_RST_CONTROLLER_CLK_OUT_ENB_X) = 0x23000780;
	CLOCK(CLK_RST_CONTROLLER_CLK_OUT_ENB_Y) = 0x300;
	CLOCK(0xF8) = 0;
	CLOCK(0xFC) = 0;
	CLOCK(0x3A0) = 0;
	CLOCK(0x3A4) = 0;
	CLOCK(0x554) = 0;
	CLOCK(0xD0) &= 0x1F7FFFFF;
	CLOCK(0x410) &= 0xFFFF3FFF;
	CLOCK(0x148) = (CLOCK(0x148) & 0x1FFFFFFF) | 0x80000000;
	CLOCK(0x180) = (CLOCK(0x180) & 0x1FFFFFFF) | 0x80000000;
	CLOCK(0x6A0) = (CLOCK(0x6A0) & 0x1FFFFFFF) | 0x80000000;
}

void config_hw()
{
	mbist_workaround();
	clock_enable_se();

	//Enable fuse clock.
	clock_enable_fuse(1);
	//Disable fuse programming.
	fuse_disable_program();

	mc_enable();

	config_oscillators();
	APB_MISC(0x40) = 0;
	config_gpios();

#ifdef DEBUG_UART_PORT
	clock_enable_uart(DEBUG_UART_PORT);
	uart_init(DEBUG_UART_PORT, 115200);
	pinmux_config_uart(DEBUG_UART_PORT);
#endif

	clock_enable_cl_dvfs();

	clock_enable_i2c(I2C_1);
	clock_enable_i2c(I2C_PWR);

	static const clock_t clock_unk1 = { 0x358, 0x360, 0x42C, 0x1F, 0, 0 };
	static const clock_t clock_unk2 = { 0x358, 0x360, 0, 0x1E, 0, 0 };
	clock_enable(&clock_unk1);
	clock_enable(&clock_unk2);

	i2c_init(I2C_1);
	i2c_init(I2C_PWR);

	i2c_send_byte(I2C_PWR, 0x3C, MAX77620_REG_CNFGBBC, 0x40);
	i2c_send_byte(I2C_PWR, 0x3C, MAX77620_REG_ONOFFCNFG1, 0x78);

	i2c_send_byte(I2C_PWR, 0x3C, MAX77620_REG_FPS_CFG0, 0x38);
	i2c_send_byte(I2C_PWR, 0x3C, MAX77620_REG_FPS_CFG1, 0x3A);
	i2c_send_byte(I2C_PWR, 0x3C, MAX77620_REG_FPS_CFG2, 0x38);
	i2c_send_byte(I2C_PWR, 0x3C, MAX77620_REG_FPS_LDO4, 0xF);
	i2c_send_byte(I2C_PWR, 0x3C, MAX77620_REG_FPS_LDO8, 0xC7);
	i2c_send_byte(I2C_PWR, 0x3C, MAX77620_REG_FPS_SD0, 0x4F);
	i2c_send_byte(I2C_PWR, 0x3C, MAX77620_REG_FPS_SD1, 0x29);
	i2c_send_byte(I2C_PWR, 0x3C, MAX77620_REG_FPS_SD3, 0x1B);

	i2c_send_byte(I2C_PWR, 0x3C, MAX77620_REG_SD0, 42); //42 = (1125000 - 600000) / 12500 -> 1.125V

	config_pmc_scratch();

	CLOCK(CLK_RST_CONTROLLER_SCLK_BURST_POLICY) = (CLOCK(CLK_RST_CONTROLLER_SCLK_BURST_POLICY) & 0xFFFF8888) | 0x3333;

	mc_config_carveout();

	sdram_init();
	//TODO: test this with LP0 wakeup.
	sdram_lp0_save_params(sdram_get_params());
}
