/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2020 Intel Corporation */

#ifndef TI953_H
#define TI953_H

struct ti953_register_write {
	u8 reg;
	u8 val;
};

struct ti953_register_devid {
	u8 reg;
	u8 val_expected;
};

/* register definition */
#define TI953_DEVICE_ID		0x0U
#define TI953_RESET_CTL		0x1
#define TI953_GENERAL_CFG	0x2
#define TI953_MODE_SEL		0x3
#define TI953_PLLCLK_CTRL	0x5
#define TI953_LOCAL_GPIO_DATA	0xd
#define TI953_GPIO_INPUT_CTRL	0xe
#define TI953_SCL_HIGH_TIME	0xbU
#define TI953_SCL_LOW_TIME	0xcU
#define TI953_MODE_SEL_DONE	BIT(3)
#define TI953_MODE_OV		BIT(4)

/* register value definition */
#define TI953_DIGITAL_RESET_1	0x2
#define TI953_GPIO0_RMTEN	0x10
#define TI953_GPIO0_OUT		0x1
#define TI953_GPIO1_OUT		(0x1 << 1)
#define TI953_GPIO_OUT_EN	0xf0
#define TI953_I2C_SCL_HIGH_TIME_STANDARD	0x7F
#define TI953_I2C_SCL_LOW_TIME_STANDARD		0x7F
#define TI953_I2C_SCL_HIGH_TIME_FAST		0x13
#define TI953_I2C_SCL_LOW_TIME_FAST		0x26
#define TI953_I2C_SCL_HIGH_TIME_FAST_PLUS	0x06
#define TI953_I2C_SCL_LOW_TIME_FAST_PLUS	0x0b

#define TI953_I2C_SPEED_STANDARD	0x1U
#define TI953_I2C_SPEED_FAST		0x2U
#define TI953_I2C_SPEED_HIGH		0x3U
#define TI953_I2C_SPEED_FAST_PLUS	0x4U

#define TI953_MODE_SYNC			0x00U
#define TI953_MODE_NONSYNC_EXT		0x02U
#define TI953_MODE_NONSYNC_INT		0x03U
#define TI953_MODE_DVP			0x05U
#define TI953_MODE_REG_OVERRIDE		0x10U

#define TI953_CONTS_CLK		0x40
#define TI953_CSI_1LANE		0x00
#define TI953_CSI_2LANE		0x10
#define TI953_CSI_4LANE		0x30
#define TI953_CSI_LANE_MASK	~(0x30)
#define TI953_CRC_TX_GEN_ENABLE	0x2
#define TI953_I2C_STRAP_MODE	0x1

static const struct ti953_register_write ti953_sync_mode_clk_settings[] = {
	/** ti960 actual REFCLK = 23Mhz
	 * to generate 27Mhz in synchronous mode
	 * CLK_OUT=f × 160 / HS_CLK_DIV × (M/N)
	 * reg 0x06 M: bit(4:0) = 1 HS_CLK_DIV: bit(7:5) = 4
	 * reg 0x07 N: bit(7:0) = 36
	 */
	{0x06, 0x41},
	{0x07, 0x24},
};

static const struct ti953_register_devid ti953_FPD3_RX_ID[] = {
	{0xf0, 0x5f},
	{0xf1, 0x55},
	{0xf2, 0x42},
	{0xf3, 0x39},
	{0xf4, 0x35},
	{0xf5, 0x33},
};

int ti953_reg_write(struct i2c_client *client, unsigned char reg, unsigned char val);
int ti953_reg_read(struct i2c_client *client, unsigned char reg, unsigned char *val);

bool ti953_detect(struct i2c_client *client);
int ti953_init(struct i2c_client *client,  unsigned int flag, int lanes);
int ti953_init_clk(struct i2c_client *client);
int32_t ti953_bus_speed(struct i2c_client *client, uint8_t i2c_speed);

#endif
