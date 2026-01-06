/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023-2025 Intel Corporation */

#ifndef MEDIA_SERDES_PDATA_H
#define MEDIA_SERDES_PDATA_H

#if IS_ENABLED(CONFIG_VIDEO_ISX031)
#include <media/i2c/isx031.h>
#endif

#if IS_ENABLED(CONFIG_VIDEO_AR0820)
#include <media/i2c/ar0820.h>
#endif

struct serdes_subdev_info {
	struct i2c_board_info board_info;
	int i2c_adapter_id;
	unsigned short rx_port;
	unsigned short phy_i2c_addr;
	unsigned short ser_alias;
	char suffix[5]; /* suffix for subdevs */
	unsigned short ser_phys_addr;
	unsigned int sensor_dt;
};

struct serdes_platform_data {
	unsigned int subdev_num;
	struct serdes_subdev_info *subdev_info;
	unsigned int reset_gpio;
	unsigned int FPD_gpio;
	char suffix;
	unsigned int link_freq_mbps;
	enum v4l2_mbus_type bus_type;
	unsigned int deser_nlanes;
	unsigned int ser_nlanes;
	unsigned int des_port;
	char ser_name[I2C_NAME_SIZE];
	struct i2c_board_info *deser_board_info;
};

#endif
