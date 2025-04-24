// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Intel Corporation

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/version.h>

#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/ti960.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>

#include "ti960-reg.h"
#include "ti953.h"

int ti953_reg_write(struct i2c_client *client, unsigned char reg, unsigned char val)
{
	int ret;
	int retry, timeout = 10;
	dev_dbg(&client->dev, "reg %x, val %x", reg, val);
	for (retry = 0; retry < timeout; retry++) {
		ret = i2c_smbus_write_byte_data(client, reg, val);
		if (ret < 0)
			usleep_range(5000, 6000);
		else
			break;
	}

	if (retry >= timeout) {
		dev_err(&client->dev, "%s:failed: reg=%2x\n", __func__, reg);
		return -EREMOTEIO;
	}

	return 0;
}

int ti953_reg_read(struct i2c_client *client, unsigned char reg, unsigned char *val)
{
	int ret, retry, timeout = 10;

	for (retry = 0; retry < timeout; retry++) {
		ret = i2c_smbus_read_byte_data(client, reg);
		if (ret < 0)
			usleep_range(5000, 6000);
		else {
			*val = ret & 0xFF;
			break;
		}
	}

	if (retry >= timeout) {
		dev_err(&client->dev, "%s:failed: reg=%2x\n", __func__, reg);
		return -EREMOTEIO;
	}

	return 0;
}

bool ti953_detect(struct i2c_client *client)
{
	bool ret = false;
	int i;
	int rval;
	unsigned char val;

	for (i = 0; i < ARRAY_SIZE(ti953_FPD3_RX_ID); i++) {
		rval = ti953_reg_read(client, ti953_FPD3_RX_ID[i].reg, &val);
		if (rval) {
			dev_err(&client->dev, "ti953 read timeout %d\n", rval);
			break;
		}
		if (val != ti953_FPD3_RX_ID[i].val_expected)
			break;
	}

	if (i == ARRAY_SIZE(ti953_FPD3_RX_ID))
		ret = true;
	else
		dev_err(&client->dev, "TI953 Probe Failed");

	return ret;
}

int ti953_init(struct i2c_client *client, unsigned int flag, int lanes)
{
	int rval;
	unsigned char val;
	unsigned int mode_flag = flag & 0x7;

	val = TI953_MODE_REG_OVERRIDE;
	switch (mode_flag) {
	case TI960_FL_INIT_SER:
		val |= TI953_MODE_SYNC;
		break;
	case TI960_FL_INIT_SER_NONSYNC_EXT:
		val |= TI953_MODE_NONSYNC_EXT;
		break;
	case TI960_FL_INIT_SER_NONSYNC_INT:
		val |= TI953_MODE_NONSYNC_INT;
		break;
	case TI960_FL_INIT_SER_DVP:
		val |= TI953_MODE_DVP;
		break;
	default:
		dev_err(&client->dev, "ti953 init mode is not supported\n");
		return -EINVAL;
	}

	rval = ti953_reg_write(client, TI953_MODE_SEL, val);
	if (rval) {
		dev_err(&client->dev, "ti953 write MODE_SEL failed\n");
		return rval;
	}
	rval = ti953_reg_read(client, TI953_MODE_SEL, &val);
	if (rval != 0) {
		dev_err(&client->dev, "ti953 read Mode value failed\n");
		return -EINVAL;
	} else if (!(val & TI953_MODE_SEL_DONE)) {
		dev_err(&client->dev, "ti953 Mode value not stabilized\n");
		return -EINVAL;
	} else
		dev_dbg(&client->dev, "ti953 read MODE_SEL (0x%x)\n", val);

	if (flag & TI960_FL_INIT_SER_CLK) {
		rval = ti953_init_clk(client);
		if (rval)
			return rval;
	}

	/* Configure ti953 CSI lane */
	val = TI953_I2C_STRAP_MODE;
	val |= TI953_CRC_TX_GEN_ENABLE;
	val |= TI953_CONTS_CLK;
	switch (lanes) {
	case 1:
		val |= TI953_CSI_1LANE;
		break;
	case 2:
		val |= TI953_CSI_2LANE;
		break;
	case 4:
		val |= TI953_CSI_4LANE;
		break;
	default:
		dev_err(&client->dev, "not expected csi lane\n");
		return -EINVAL;
	}

	rval = ti953_reg_write(client, TI953_GENERAL_CFG, val);
	if (rval) {
		dev_err(&client->dev, "ti953 write GENERAL_CFG failed\n");
		return rval;
	}

	rval = ti953_reg_read(client, TI953_GENERAL_CFG, &val);
	if (rval == 0)
		dev_dbg(&client->dev, "ti953 read general CFG (0x%x)\n", val);

	return 0;
}

int ti953_init_clk(struct i2c_client *client)
{
	int i, rval;

	for (i = 0; i < ARRAY_SIZE(ti953_sync_mode_clk_settings); i++) {
		rval = ti953_reg_write(client, ti953_sync_mode_clk_settings[i].reg,
					ti953_sync_mode_clk_settings[i].val);
		if (rval) {
			dev_err(&client->dev, "ti953 write CLK failed\n");
			return rval;
		}
	}

	return 0;
}

int32_t ti953_bus_speed(struct i2c_client *client, uint8_t i2c_speed)
{
	struct ti953_register_write scl_high_reg;
	struct ti953_register_write scl_low_reg;
	int32_t ret = 0;

	scl_high_reg.reg = TI953_SCL_HIGH_TIME;
	scl_low_reg.reg = TI953_SCL_LOW_TIME;
	switch (i2c_speed) {
	case TI953_I2C_SPEED_STANDARD:
		scl_high_reg.val = TI953_I2C_SCL_HIGH_TIME_STANDARD;
		scl_low_reg.val = TI953_I2C_SCL_LOW_TIME_STANDARD;
		break;
	case TI953_I2C_SPEED_FAST:
		scl_high_reg.val = TI953_I2C_SCL_HIGH_TIME_FAST;
		scl_low_reg.val = TI953_I2C_SCL_LOW_TIME_FAST;
		break;
	case TI953_I2C_SPEED_FAST_PLUS:
		scl_high_reg.val = TI953_I2C_SCL_HIGH_TIME_FAST_PLUS;
		scl_low_reg.val = TI953_I2C_SCL_LOW_TIME_FAST_PLUS;
		break;
	case TI953_I2C_SPEED_HIGH:
	default:
		dev_err(&client->dev, "ti953 unsupported I2C speed mode %u", i2c_speed);
		scl_high_reg.val = TI953_I2C_SCL_HIGH_TIME_STANDARD;
		scl_low_reg.val = TI953_I2C_SCL_LOW_TIME_STANDARD;
		ret = -EINVAL;
		break;
	}
	if (ret != 0)
		return ret;
	ret = ti953_reg_write(client, scl_high_reg.reg, scl_high_reg.val);
	if (ret != 0) {
		dev_err(&client->dev, "ti953 write SCL_HIGH_TIME failed %d", ret);
		return ret;
	}
	ret = ti953_reg_write(client,
			      scl_low_reg.reg, scl_low_reg.val);
	if (ret != 0) {
		dev_err(&client->dev, " ti953 write SCL_LOW_TIME failed %d", ret);
		return ret;
	}

	return 0;
}
