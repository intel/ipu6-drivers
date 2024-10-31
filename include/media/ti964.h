/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2016 - 2020 Intel Corporation */

#ifndef TI964_H
#define TI964_H

#include <linux/i2c.h>
#include <linux/regmap.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>

#define TI964_NAME "ti964"

#define PIXEL_ORDER_GRBG	0
#define PIXEL_ORDER_RGGB	1
#define PIXEL_ORDER_BGGR	2
#define PIXEL_ORDER_GBRG	3

#define NR_OF_TI964_STREAMS	4
#define NR_OF_TI964_SOURCE_PADS	1
#define NR_OF_TI964_SINK_PADS	4
#define NR_OF_TI964_PADS \
	(NR_OF_TI964_SOURCE_PADS + NR_OF_TI964_SINK_PADS)
#define NR_OF_GPIOS_PER_PORT	2
#define NR_OF_TI964_GPIOS	\
	(NR_OF_TI964_SINK_PADS * NR_OF_GPIOS_PER_PORT)

#define TI964_PAD_SOURCE	4

#define TI964_MIN_WIDTH		640
#define TI964_MIN_HEIGHT	480
#define TI964_MAX_WIDTH		1920
#define TI964_MAX_HEIGHT	1080

struct ti964_csi_data_format {
	u32 code;
	u8 width;
	u8 compressed;
	u8 pixel_order;
	u8 mipi_dt_code;
};

struct ti964_subdev_info {
	struct i2c_board_info board_info;
	int i2c_adapter_id;
	unsigned short rx_port;
	unsigned short phy_i2c_addr;
	const char suffix; /* suffix for subdevs */
};

struct ti964_pdata {
	unsigned int subdev_num;
	struct ti964_subdev_info *subdev_info;
	unsigned int reset_gpio;
	const char suffix; /* suffix for multi aggregators, abcd... */
};

struct ti964_platform_data {
	unsigned short i2c_addr;
	unsigned short i2c_adapter;

	/* specify gpio pins of Deser for PWDN, FSIN, RESET. */
	int xshutdown;

	char module_name[16]; /* module name from ACPI */
	char suffix; /* suffix to identify multi sensors, abcd.. */
	unsigned int high_framevalid_flags; /* high framevaild flags*/
};

#endif
