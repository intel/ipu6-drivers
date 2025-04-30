// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2025 Intel Corporation.

#include <asm/unaligned.h>
#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/ov2311.h>

#define to_ov2311(_sd)			container_of(_sd, struct ov2311, sd)

#define OV2311_REG_CHIP_ID		0x300B
#define OV2311_CHIP_ID			0x11

#define OV2311_LINK_FREQ_800MHZ		800000000ULL
#define OV2311_SCLK			80000000LL
#define OV2311_DATA_LANES		2
#define OV2311_RGB_DEPTH		10

#define OV2311_REG_MODE_SELECT		0x0100
#define OV2311_MODE_STANDBY		0x00
#define OV2311_MODE_STREAMING		0x01

/* vertical-timings from sensor */
#define OV2311_REG_VTS			0x380E
#define OV2311_VTS_DEF			0x5c2
#define OV2311_VTS_MIN			0x5c2
#define OV2311_VTS_MAX			0x7fff

/* horizontal-timings from sensor */
#define OV2311_REG_HTS			0x380C
#define OV2311_HTS_DEF			0x710

/* Analog gain controls from sensor */
#define OV2311_REG_ANALOG_GAIN		0x3508
#define OV2311_ANAL_GAIN_MIN		128
#define OV2311_ANAL_GAIN_MAX		1983
#define OV2311_ANAL_GAIN_STEP		1

/* Digital gain controls from sensor */
#define OV2311_REG_DIGITAL_GAIN_COARSE	0x350a
#define OV2311_REG_DIGITAL_GAIN_FINE_H	0x500c
#define OV2311_REG_DIGITAL_GAIN_FINE_L	0x500e
#define OV2311_DGTL_GAIN_MIN		1024
#define OV2311_DGTL_GAIN_MAX		4095
#define OV2311_DGTL_GAIN_STEP		1
#define OV2311_DGTL_GAIN_DEF		1024

/* Exposure controls from sensor */
#define OV2311_REG_EXPOSURE		0x3501
#define OV2311_EXPOSURE_MIN		2
#define OV2311_EXPOSURE_MAX		0x510
#define OV2311_EXPOSURE_DEF		0x510
#define OV2311_EXPOSURE_STEP		1

/* Test Pattern Control */
#define OV2311_REG_TEST_PATTERN		0X5E00
#define OV2311_TEST_PATTERN_ENABLE	BIT(7)
#define OV2311_TEST_PATTERN_BAR_SHIFT	2

/* Group Access */
#define OV2311_REG_GROUP_ACCESS		0x3208
#define OV2311_GROUP_HOLD_START		0x0
#define OV2311_GROUP_HOLD_END		0x10
#define OV2311_GROUP_HOLD_LAUNCH	0xa0

enum {
	OV2311_LINK_FREQ_800MHZ_INDEX,
};

struct ov2311_reg {
	enum {
		OV2311_REG_LEN_DELAY = 0,
		OV2311_REG_LEN_08BIT = 1,
		OV2311_REG_LEN_16BIT = 2,
	} mode;
	u16 address;
	u16 val;
};

struct ov2311_reg_list {
	u32 num_of_regs;
	const struct ov2311_reg *regs;
};

struct ov2311_link_freq_config {
	const struct ov2311_reg_list reg_list;
};

struct ov2311_mode {
	/* Frame width in pixels */
	u32 width;

	/* Frame height in pixels */
	u32 height;

	/* MEDIA_BUS_FMT */
	u32 code;

	/* MODE_FPS*/
	u32 fps;

	/* Horizontal timining size */
	u32 hts;

	/* Default vertical timining size */
	u32 vts_def;

	/* Min vertical timining size */
	u32 vts_min;

	/* Max vertical timining size */
	u32 vts_max;

	/* Link frequency needed for this resolution */
	u32 link_freq_index;

	/* MIPI_LANES */
	s32 lanes;

	/* bit per pixel */
	u32 bpp;

	/* Sensor register settings for this resolution */
	const struct ov2311_reg_list reg_list;
};

struct ov2311 {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;

	/* V4L2 Controls */
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *analogue_gain;
	struct v4l2_ctrl *digital_gain;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *exposure;

	/* Current mode */
	const struct ov2311_mode *cur_mode;
	/* Previous mode */
	const struct ov2311_mode *pre_mode;

	/* To serialize asynchronus callbacks */
	struct mutex mutex;

	/* i2c client */
	struct i2c_client *client;

	struct ov2311_platform_data *platform_data;

	/* Streaming on/off */
	bool streaming;
};

static const struct ov2311_reg ov2311_mode965_1600_1300_reg[] = {
	{OV2311_REG_LEN_08BIT, 0x0103, 0x01},
	{OV2311_REG_LEN_08BIT, 0x0100, 0x00},
	{OV2311_REG_LEN_08BIT, 0x010c, 0x02},
	{OV2311_REG_LEN_08BIT, 0x010b, 0x01},
	{OV2311_REG_LEN_08BIT, 0x0300, 0x01},
	{OV2311_REG_LEN_08BIT, 0x0302, 0x32},
	{OV2311_REG_LEN_08BIT, 0x0303, 0x00},
	{OV2311_REG_LEN_08BIT, 0x0304, 0x03},
	{OV2311_REG_LEN_08BIT, 0x0305, 0x02},
	{OV2311_REG_LEN_08BIT, 0x0306, 0x01},
	{OV2311_REG_LEN_08BIT, 0x3001, 0x02},
	{OV2311_REG_LEN_08BIT, 0x3004, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3005, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3006, 0x04},
	{OV2311_REG_LEN_08BIT, 0x3014, 0x04},
	{OV2311_REG_LEN_08BIT, 0x301c, 0xf0},
	{OV2311_REG_LEN_08BIT, 0x302c, 0x00},
	{OV2311_REG_LEN_08BIT, 0x302d, 0x12},
	{OV2311_REG_LEN_08BIT, 0x302e, 0x4c},
	{OV2311_REG_LEN_08BIT, 0x302f, 0x8c},
	{OV2311_REG_LEN_08BIT, 0x3030, 0x10},
	{OV2311_REG_LEN_08BIT, 0x303f, 0x03},
	{OV2311_REG_LEN_08BIT, 0x3103, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3106, 0x08},
	{OV2311_REG_LEN_08BIT, 0x31ff, 0x01},
	{OV2311_REG_LEN_08BIT, 0x3501, 0xdc},
	{OV2311_REG_LEN_08BIT, 0x3502, 0xba},
	{OV2311_REG_LEN_08BIT, 0x3506, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3507, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3508, 0x04},
	{OV2311_REG_LEN_08BIT, 0x3620, 0x67},
	{OV2311_REG_LEN_08BIT, 0x3666, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3674, 0x10},
	{OV2311_REG_LEN_08BIT, 0x3675, 0x00},
	{OV2311_REG_LEN_08BIT, 0x36a2, 0x04},
	{OV2311_REG_LEN_08BIT, 0x36a3, 0x80},
	{OV2311_REG_LEN_08BIT, 0x36b0, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3712, 0x00},
	{OV2311_REG_LEN_08BIT, 0x379b, 0x01},
	{OV2311_REG_LEN_08BIT, 0x379c, 0x10},
	{OV2311_REG_LEN_08BIT, 0x3800, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3801, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3802, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3803, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3804, 0x06},
	{OV2311_REG_LEN_08BIT, 0x3805, 0x4f},
	{OV2311_REG_LEN_08BIT, 0x3806, 0x05},
	{OV2311_REG_LEN_08BIT, 0x3807, 0x23},
	{OV2311_REG_LEN_08BIT, 0x3808, 0x06},
	{OV2311_REG_LEN_08BIT, 0x3809, 0x40},
	{OV2311_REG_LEN_08BIT, 0x380a, 0x05},
	{OV2311_REG_LEN_08BIT, 0x380b, 0x14},
	{OV2311_REG_LEN_08BIT, 0x380c, 0x07},
	{OV2311_REG_LEN_08BIT, 0x380d, 0x10},
	{OV2311_REG_LEN_08BIT, 0x380e, 0x05},
	{OV2311_REG_LEN_08BIT, 0x380f, 0xc2},
	{OV2311_REG_LEN_08BIT, 0x3810, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3811, 0x08},
	{OV2311_REG_LEN_08BIT, 0x3812, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3813, 0x08},
	{OV2311_REG_LEN_08BIT, 0x3814, 0x11},
	{OV2311_REG_LEN_08BIT, 0x3815, 0x11},
	{OV2311_REG_LEN_08BIT, 0x3816, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3817, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3818, 0x04},
	{OV2311_REG_LEN_08BIT, 0x3819, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3820, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3821, 0x00},
	{OV2311_REG_LEN_08BIT, 0x382c, 0x09},
	{OV2311_REG_LEN_08BIT, 0x382d, 0x9a},
	{OV2311_REG_LEN_08BIT, 0x3882, 0x02},
	{OV2311_REG_LEN_08BIT, 0x3883, 0x60},
	{OV2311_REG_LEN_08BIT, 0x3885, 0x01},
	{OV2311_REG_LEN_08BIT, 0x389d, 0x03},
	{OV2311_REG_LEN_08BIT, 0x38a6, 0x00},
	{OV2311_REG_LEN_08BIT, 0x38a7, 0x01},
	{OV2311_REG_LEN_08BIT, 0x38b3, 0x01},
	{OV2311_REG_LEN_08BIT, 0x38b1, 0x00},
	{OV2311_REG_LEN_08BIT, 0x38e8, 0x03},
	{OV2311_REG_LEN_08BIT, 0x3920, 0xa5},
	{OV2311_REG_LEN_08BIT, 0x3921, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3922, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3923, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3924, 0x05},
	{OV2311_REG_LEN_08BIT, 0x3925, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3926, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3927, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3928, 0x1a},
	{OV2311_REG_LEN_08BIT, 0x392d, 0x05},
	{OV2311_REG_LEN_08BIT, 0x392e, 0xf2},
	{OV2311_REG_LEN_08BIT, 0x392f, 0x40},
	{OV2311_REG_LEN_08BIT, 0x4001, 0x00},
	{OV2311_REG_LEN_08BIT, 0x4003, 0x40},
	{OV2311_REG_LEN_08BIT, 0x4008, 0x12},
	{OV2311_REG_LEN_08BIT, 0x4009, 0x1b},
	{OV2311_REG_LEN_08BIT, 0x400c, 0x0c},
	{OV2311_REG_LEN_08BIT, 0x400d, 0x13},
	{OV2311_REG_LEN_08BIT, 0x4010, 0x44},
	{OV2311_REG_LEN_08BIT, 0x4011, 0x00},
	{OV2311_REG_LEN_08BIT, 0x4042, 0x11},
	{OV2311_REG_LEN_08BIT, 0x4409, 0x5f},
	{OV2311_REG_LEN_08BIT, 0x4600, 0x00},
	{OV2311_REG_LEN_08BIT, 0x4601, 0xa0},
	{OV2311_REG_LEN_08BIT, 0x4800, 0x00},
	{OV2311_REG_LEN_08BIT, 0x4f00, 0x00},
	{OV2311_REG_LEN_08BIT, 0x4f07, 0x00},
	{OV2311_REG_LEN_08BIT, 0x4f08, 0x03},
	{OV2311_REG_LEN_08BIT, 0x4f09, 0x08},
	{OV2311_REG_LEN_08BIT, 0x4f0c, 0x06},
	{OV2311_REG_LEN_08BIT, 0x4f0d, 0x02},
	{OV2311_REG_LEN_08BIT, 0x4f10, 0x07},
	{OV2311_REG_LEN_08BIT, 0x4f11, 0x00},
	{OV2311_REG_LEN_08BIT, 0x4f12, 0xf0},
	{OV2311_REG_LEN_08BIT, 0x4f13, 0xeb},
	{OV2311_REG_LEN_08BIT, 0x5000, 0x9f},
	{OV2311_REG_LEN_08BIT, 0x5e00, 0x00},
	{OV2311_REG_LEN_08BIT, 0x5e01, 0x41},
	{OV2311_REG_LEN_08BIT, 0x3006, 0x08},
	{OV2311_REG_LEN_08BIT, 0x3004, 0x02},
	{OV2311_REG_LEN_08BIT, 0x3007, 0x02},
	{OV2311_REG_LEN_08BIT, 0x301c, 0x20},
	{OV2311_REG_LEN_08BIT, 0x3020, 0x20},
	{OV2311_REG_LEN_08BIT, 0x3025, 0x02},
	{OV2311_REG_LEN_08BIT, 0x382c, 0x0a},
	{OV2311_REG_LEN_08BIT, 0x382d, 0xf8},
	{OV2311_REG_LEN_08BIT, 0x3920, 0xff},
	{OV2311_REG_LEN_08BIT, 0x3921, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3923, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3924, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3925, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3926, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3927, 0x01},
	{OV2311_REG_LEN_08BIT, 0x3928, 0x62},
	{OV2311_REG_LEN_08BIT, 0x392b, 0x00},
	{OV2311_REG_LEN_08BIT, 0x392c, 0x00},
	{OV2311_REG_LEN_08BIT, 0x392d, 0x03},
	{OV2311_REG_LEN_08BIT, 0x392e, 0x88},
	{OV2311_REG_LEN_08BIT, 0x392f, 0x0b},
	{OV2311_REG_LEN_08BIT, 0x38b3, 0x07},
	{OV2311_REG_LEN_08BIT, 0x3885, 0x07},
	{OV2311_REG_LEN_08BIT, 0x382b, 0x32},
	{OV2311_REG_LEN_08BIT, 0x3670, 0x68},
};

static const struct ov2311_reg ov2311_mode392_1600_1300_reg[] = {
	{OV2311_REG_LEN_08BIT, 0x0103, 0x01},
	{OV2311_REG_LEN_08BIT, 0x0100, 0x00},
	{OV2311_REG_LEN_08BIT, 0x010c, 0x02},
	{OV2311_REG_LEN_08BIT, 0x010b, 0x01},
	{OV2311_REG_LEN_08BIT, 0x0300, 0x01},
	{OV2311_REG_LEN_08BIT, 0x0302, 0x32},
	{OV2311_REG_LEN_08BIT, 0x0303, 0x00},
	{OV2311_REG_LEN_08BIT, 0x0304, 0x03},
	{OV2311_REG_LEN_08BIT, 0x0305, 0x02},
	{OV2311_REG_LEN_08BIT, 0x0306, 0x01},
	{OV2311_REG_LEN_08BIT, 0x3001, 0x02},
	{OV2311_REG_LEN_08BIT, 0x3004, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3005, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3006, 0x04},
	{OV2311_REG_LEN_08BIT, 0x3014, 0x04},
	{OV2311_REG_LEN_08BIT, 0x301c, 0xf0},
	{OV2311_REG_LEN_08BIT, 0x302c, 0x00},
	{OV2311_REG_LEN_08BIT, 0x302d, 0x12},
	{OV2311_REG_LEN_08BIT, 0x302e, 0x4c},
	{OV2311_REG_LEN_08BIT, 0x302f, 0x8c},
	{OV2311_REG_LEN_08BIT, 0x3030, 0x10},
	{OV2311_REG_LEN_08BIT, 0x303f, 0x03},
	{OV2311_REG_LEN_08BIT, 0x3103, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3106, 0x08},
	{OV2311_REG_LEN_08BIT, 0x31ff, 0x01},
	{OV2311_REG_LEN_08BIT, 0x3501, 0xdc},
	{OV2311_REG_LEN_08BIT, 0x3502, 0xba},
	{OV2311_REG_LEN_08BIT, 0x3506, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3507, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3508, 0x04},
	{OV2311_REG_LEN_08BIT, 0x3620, 0x67},
	{OV2311_REG_LEN_08BIT, 0x3666, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3674, 0x10},
	{OV2311_REG_LEN_08BIT, 0x3675, 0x00},
	{OV2311_REG_LEN_08BIT, 0x36a2, 0x04},
	{OV2311_REG_LEN_08BIT, 0x36a3, 0x80},
	{OV2311_REG_LEN_08BIT, 0x36b0, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3712, 0x00},
	{OV2311_REG_LEN_08BIT, 0x379b, 0x01},
	{OV2311_REG_LEN_08BIT, 0x379c, 0x10},
	{OV2311_REG_LEN_08BIT, 0x3800, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3801, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3802, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3803, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3804, 0x06},
	{OV2311_REG_LEN_08BIT, 0x3805, 0x4f},
	{OV2311_REG_LEN_08BIT, 0x3806, 0x05},
	{OV2311_REG_LEN_08BIT, 0x3807, 0x23},
	{OV2311_REG_LEN_08BIT, 0x3808, 0x06},
	{OV2311_REG_LEN_08BIT, 0x3809, 0x40},
	{OV2311_REG_LEN_08BIT, 0x380a, 0x05},
	{OV2311_REG_LEN_08BIT, 0x380b, 0x14},
	{OV2311_REG_LEN_08BIT, 0x380c, 0x07},
	{OV2311_REG_LEN_08BIT, 0x380d, 0x10},
	{OV2311_REG_LEN_08BIT, 0x380e, 0x05},
	{OV2311_REG_LEN_08BIT, 0x380f, 0xc2},
	{OV2311_REG_LEN_08BIT, 0x3810, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3811, 0x08},
	{OV2311_REG_LEN_08BIT, 0x3812, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3813, 0x08},
	{OV2311_REG_LEN_08BIT, 0x3814, 0x11},
	{OV2311_REG_LEN_08BIT, 0x3815, 0x11},
	{OV2311_REG_LEN_08BIT, 0x3816, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3817, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3818, 0x04},
	{OV2311_REG_LEN_08BIT, 0x3819, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3820, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3821, 0x00},
	{OV2311_REG_LEN_08BIT, 0x382c, 0x09},
	{OV2311_REG_LEN_08BIT, 0x382d, 0x9a},
	{OV2311_REG_LEN_08BIT, 0x3882, 0x02},
	{OV2311_REG_LEN_08BIT, 0x3883, 0x60},
	{OV2311_REG_LEN_08BIT, 0x3885, 0x01},
	{OV2311_REG_LEN_08BIT, 0x389d, 0x03},
	{OV2311_REG_LEN_08BIT, 0x38a6, 0x00},
	{OV2311_REG_LEN_08BIT, 0x38a7, 0x01},
	{OV2311_REG_LEN_08BIT, 0x38b3, 0x01},
	{OV2311_REG_LEN_08BIT, 0x38b1, 0x00},
	{OV2311_REG_LEN_08BIT, 0x38e8, 0x03},
	{OV2311_REG_LEN_08BIT, 0x3920, 0xa5},
	{OV2311_REG_LEN_08BIT, 0x3921, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3922, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3923, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3924, 0x05},
	{OV2311_REG_LEN_08BIT, 0x3925, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3926, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3927, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3928, 0x1a},
	{OV2311_REG_LEN_08BIT, 0x392d, 0x05},
	{OV2311_REG_LEN_08BIT, 0x392e, 0xf2},
	{OV2311_REG_LEN_08BIT, 0x392f, 0x40},
	{OV2311_REG_LEN_08BIT, 0x4001, 0x00},
	{OV2311_REG_LEN_08BIT, 0x4003, 0x40},
	{OV2311_REG_LEN_08BIT, 0x4008, 0x12},
	{OV2311_REG_LEN_08BIT, 0x4009, 0x1b},
	{OV2311_REG_LEN_08BIT, 0x400c, 0x0c},
	{OV2311_REG_LEN_08BIT, 0x400d, 0x13},
	{OV2311_REG_LEN_08BIT, 0x4010, 0x44},
	{OV2311_REG_LEN_08BIT, 0x4011, 0x00},
	{OV2311_REG_LEN_08BIT, 0x4042, 0x11},
	{OV2311_REG_LEN_08BIT, 0x4409, 0x5f},
	{OV2311_REG_LEN_08BIT, 0x4600, 0x00},
	{OV2311_REG_LEN_08BIT, 0x4601, 0xa0},
	{OV2311_REG_LEN_08BIT, 0x4800, 0x00},
	{OV2311_REG_LEN_08BIT, 0x4f00, 0x00},
	{OV2311_REG_LEN_08BIT, 0x4f07, 0x00},
	{OV2311_REG_LEN_08BIT, 0x4f08, 0x03},
	{OV2311_REG_LEN_08BIT, 0x4f09, 0x08},
	{OV2311_REG_LEN_08BIT, 0x4f0c, 0x06},
	{OV2311_REG_LEN_08BIT, 0x4f0d, 0x02},
	{OV2311_REG_LEN_08BIT, 0x4f10, 0x07},
	{OV2311_REG_LEN_08BIT, 0x4f11, 0x00},
	{OV2311_REG_LEN_08BIT, 0x4f12, 0xf0},
	{OV2311_REG_LEN_08BIT, 0x4f13, 0xeb},
	{OV2311_REG_LEN_08BIT, 0x5000, 0x9f},
	{OV2311_REG_LEN_08BIT, 0x5e00, 0x00},
	{OV2311_REG_LEN_08BIT, 0x5e01, 0x41},
	{OV2311_REG_LEN_08BIT, 0x3006, 0x08},
	{OV2311_REG_LEN_08BIT, 0x3004, 0x02},
	{OV2311_REG_LEN_08BIT, 0x3007, 0x02},
	{OV2311_REG_LEN_08BIT, 0x301c, 0x20},
	{OV2311_REG_LEN_08BIT, 0x3020, 0x20},
	{OV2311_REG_LEN_08BIT, 0x3025, 0x02},
	{OV2311_REG_LEN_08BIT, 0x382c, 0x0a},
	{OV2311_REG_LEN_08BIT, 0x382d, 0xf8},
	{OV2311_REG_LEN_08BIT, 0x3920, 0xff},
	{OV2311_REG_LEN_08BIT, 0x3921, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3923, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3924, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3925, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3926, 0x00},
	{OV2311_REG_LEN_08BIT, 0x3927, 0x01},
	{OV2311_REG_LEN_08BIT, 0x3928, 0xba},
	{OV2311_REG_LEN_08BIT, 0x392b, 0x00},
	{OV2311_REG_LEN_08BIT, 0x392c, 0x00},
	{OV2311_REG_LEN_08BIT, 0x392d, 0x03},
	{OV2311_REG_LEN_08BIT, 0x392e, 0x88},
	{OV2311_REG_LEN_08BIT, 0x392f, 0x0b},
	{OV2311_REG_LEN_08BIT, 0x38b3, 0x07},
	{OV2311_REG_LEN_08BIT, 0x3885, 0x07},
	{OV2311_REG_LEN_08BIT, 0x382b, 0x32},
	{OV2311_REG_LEN_08BIT, 0x3670, 0x68},
};

static const struct ov2311_reg_list ov2311_mode965_1600_1300_reg_list = {
	.num_of_regs = ARRAY_SIZE(ov2311_mode965_1600_1300_reg),
	.regs = ov2311_mode965_1600_1300_reg,
};

static const struct ov2311_reg_list ov2311_mode392_1600_1300_reg_list = {
	.num_of_regs = ARRAY_SIZE(ov2311_mode392_1600_1300_reg),
	.regs = ov2311_mode392_1600_1300_reg,
};

static const s64 link_freq_menu_items[] = {
	OV2311_LINK_FREQ_800MHZ,
};

static u32 supported_formats[] = {
	MEDIA_BUS_FMT_SGRBG10_1X10,
};

static const char * const ov2311_test_pattern_menu[] = {
	"Disabled",
	"Standard Color Bar",
	"Top-bottom Darker Bar",
	"Left-right Darker Bar",
	"Bottom-top Darker Bar",
};

static const struct ov2311_mode supported_modes[] = {
	{
		.width = 1600,
		.height = 1300,
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.bpp = 10,
		.hts = OV2311_HTS_DEF,
		.vts_def = OV2311_VTS_DEF,
		.vts_min = OV2311_VTS_MIN,
		.vts_max = OV2311_VTS_MAX,
		.reg_list = ov2311_mode965_1600_1300_reg_list,
		.link_freq_index = OV2311_LINK_FREQ_800MHZ_INDEX,
	},
};

static int ov2311_read_reg(struct ov2311 *ov2311, u16 reg, u16 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov2311->sd);
	struct i2c_msg msgs[2];
	u8 addr_buf[2];
	u8 data_buf[4] = {0};
	int ret;

	if (len > 4)
		return -EINVAL;

	put_unaligned_be16(reg, addr_buf);
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(addr_buf);
	msgs[0].buf = addr_buf;
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_buf[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = get_unaligned_be32(data_buf);

	return 0;
}

static int ov2311_write_reg(struct ov2311 *ov2311, u16 reg, u16 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov2311->sd);
	u8 buf[6];

	if (len > 4)
		return -EINVAL;

	dev_dbg(&client->dev, "%s, reg %x len %x, val %x\n", __func__, reg, len, val);
	put_unaligned_be16(reg, buf);
	put_unaligned_be32(val << 8 * (4 - len), buf + 2);
	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

static int ov2311_write_reg_list(struct ov2311 *ov2311,
				 const struct ov2311_reg_list *r_list)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov2311->sd);
	unsigned int i;
	int ret;

	for (i = 0; i < r_list->num_of_regs; i++) {
		if (r_list->regs[i].mode == OV2311_REG_LEN_DELAY) {
			msleep(r_list->regs[i].val);
			continue;
		}
		ret = ov2311_write_reg(ov2311, r_list->regs[i].address,
				       OV2311_REG_LEN_08BIT,
				       r_list->regs[i].val);
		if (ret) {
			dev_err_ratelimited(&client->dev,
				"failed to write reg 0x%4.4x. error = %d",
				r_list->regs[i].address, ret);
			return ret;
		}
	}

	return 0;
}

static int ov2311_test_pattern(struct ov2311 *ov2311, u32 pattern)
{
	if (pattern)
		pattern = pattern << OV2311_TEST_PATTERN_BAR_SHIFT |
			  OV2311_TEST_PATTERN_ENABLE;

	return ov2311_write_reg(ov2311, OV2311_REG_TEST_PATTERN, 1, pattern);
}

static int ov2311_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov2311 *ov2311 = container_of(ctrl->handler,
					     struct ov2311, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&ov2311->sd);
	s64 exposure_max;
	int ret = 0;

	/* Propagate change of current control to all related controls */
	if (ctrl->id == V4L2_CID_VBLANK) {
		/* Update max exposure while meeting expected vblanking */
		exposure_max = ov2311->cur_mode->height + ctrl->val -
			       OV2311_EXPOSURE_MAX;
		__v4l2_ctrl_modify_range(ov2311->exposure,
					 ov2311->exposure->minimum,
					 exposure_max, ov2311->exposure->step,
					 exposure_max);
	}

	/* V4L2 controls values will be applied only when power is already up */
	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	if (ret)
		dev_dbg(&client->dev, "failed to hold command");

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		ret = ov2311_write_reg(ov2311, OV2311_REG_ANALOG_GAIN, 2,
				       ctrl->val);
		break;

	case V4L2_CID_DIGITAL_GAIN:
		ret = 0;
		break;

	case V4L2_CID_EXPOSURE:
		ret = ov2311_write_reg(ov2311, OV2311_REG_EXPOSURE, 2, ctrl->val);
		break;

	case V4L2_CID_VBLANK:
		ret = ov2311_write_reg(ov2311, OV2311_REG_VTS, 2,
				       ov2311->cur_mode->height + ctrl->val);
		break;

	case V4L2_CID_TEST_PATTERN:
		ret = ov2311_test_pattern(ov2311, ctrl->val);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops ov2311_ctrl_ops = {
	.s_ctrl = ov2311_set_ctrl,
};

static int ov2311_init_controls(struct ov2311 *ov2311)
{
	struct v4l2_ctrl_handler *ctrl_hdlr;
	u64 hblank;
	int ret;

	ctrl_hdlr = &ov2311->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 8);
	if (ret)
		return ret;

	ctrl_hdlr->lock = &ov2311->mutex;
	ov2311->link_freq = v4l2_ctrl_new_int_menu(ctrl_hdlr, &ov2311_ctrl_ops,
					   V4L2_CID_LINK_FREQ,
					   ARRAY_SIZE(link_freq_menu_items) - 1,
					   0, link_freq_menu_items);
	if (ov2311->link_freq)
		ov2311->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	ov2311->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &ov2311_ctrl_ops,
			  V4L2_CID_VBLANK,
			  0,
			  OV2311_VTS_MAX - ov2311->cur_mode->height, 1,
			  ov2311->cur_mode->vts_def - ov2311->cur_mode->height);

	ov2311->analogue_gain = v4l2_ctrl_new_std(ctrl_hdlr, &ov2311_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  OV2311_ANAL_GAIN_MIN, OV2311_ANAL_GAIN_MAX,
			  OV2311_ANAL_GAIN_STEP, OV2311_ANAL_GAIN_MIN);

	ov2311->digital_gain = v4l2_ctrl_new_std(ctrl_hdlr, &ov2311_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			OV2311_DGTL_GAIN_MIN, OV2311_DGTL_GAIN_MAX,
			OV2311_DGTL_GAIN_STEP, OV2311_DGTL_GAIN_DEF);

	ov2311->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &ov2311_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     OV2311_EXPOSURE_MIN,
					     OV2311_EXPOSURE_MAX,
					     OV2311_EXPOSURE_STEP,
					     OV2311_EXPOSURE_DEF);

	ov2311->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &ov2311_ctrl_ops,
					       V4L2_CID_PIXEL_RATE, 0,
					       OV2311_SCLK, 1, OV2311_SCLK);

	if (ov2311->pixel_rate)
		ov2311->pixel_rate->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	hblank = ov2311->cur_mode->hts - ov2311->cur_mode->width;
	ov2311->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &ov2311_ctrl_ops,
					   V4L2_CID_HBLANK, hblank, hblank, 1,
					   hblank);
	if (ov2311->hblank)
		ov2311->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	if (ctrl_hdlr->error)
		return ctrl_hdlr->error;

	ov2311->sd.ctrl_handler = ctrl_hdlr;

	return 0;
}

static void ov2311_update_pad_format(const struct ov2311_mode *mode,
				     struct v4l2_mbus_framefmt *fmt)
{
	fmt->width = mode->width;
	fmt->height = mode->height;
	fmt->code = mode->code;
	fmt->field = V4L2_FIELD_NONE;
}

static int ov2311_start_streaming(struct ov2311 *ov2311)
{
	int retry, ret;
	struct i2c_client *client = v4l2_get_subdevdata(&ov2311->sd);
	const struct ov2311_reg_list *reg_list;

	if (ov2311->cur_mode != ov2311->pre_mode) {
		reg_list = &ov2311->cur_mode->reg_list;
		ret = ov2311_write_reg_list(ov2311, reg_list);
		if (ret) {
			dev_err(&client->dev, "failed to set stream mode");
			return ret;
		}
		ov2311->pre_mode = ov2311->cur_mode;
	} else {
		dev_dbg(&client->dev, "same mode, skip write reg list");
	}
	retry = 50;
	do {
		ret = ov2311_write_reg(ov2311, OV2311_REG_MODE_SELECT,
				 OV2311_REG_LEN_08BIT,
				 OV2311_MODE_STREAMING);
		if (ret)
			dev_err(&client->dev, "retry to write STREAMING");
	} while (ret && retry--);

	if (ret) {
		dev_err(&client->dev, "failed to start stream");
		return ret;
	}

	return 0;
}

static void ov2311_stop_streaming(struct ov2311 *ov2311)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov2311->sd);

	if (ov2311_write_reg(ov2311, OV2311_REG_MODE_SELECT, 1,
			     OV2311_MODE_STANDBY))
		dev_err(&client->dev, "failed to stop stream");
	msleep(50);
}

static int ov2311_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov2311 *ov2311 = to_ov2311(sd);
	struct i2c_client *client = v4l2_get_subdevdata(&ov2311->sd);
	int ret = 0;

	if (ov2311->streaming == enable)
		return 0;

	mutex_lock(&ov2311->mutex);
	if (enable) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			mutex_unlock(&ov2311->mutex);
			return ret;
		}

		ret = ov2311_start_streaming(ov2311);
		if (ret) {
			enable = 0;
			ov2311_stop_streaming(ov2311);
			pm_runtime_put(&client->dev);
		}
	} else {
		ov2311_stop_streaming(ov2311);
		pm_runtime_put(&client->dev);
	}

	ov2311->streaming = enable;

	mutex_unlock(&ov2311->mutex);

	return ret;
}

static int __maybe_unused ov2311_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov2311 *ov2311 = to_ov2311(sd);

	mutex_lock(&ov2311->mutex);
	if (ov2311->streaming)
		ov2311_stop_streaming(ov2311);

	mutex_unlock(&ov2311->mutex);

	return 0;
}

static int __maybe_unused ov2311_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov2311 *ov2311 = to_ov2311(sd);
	int ret;

	mutex_lock(&ov2311->mutex);
	if (ov2311->streaming) {
		ret = ov2311_start_streaming(ov2311);
		if (ret) {
			ov2311->streaming = false;
			ov2311_stop_streaming(ov2311);
			mutex_unlock(&ov2311->mutex);
			return ret;
		}
	}

	mutex_unlock(&ov2311->mutex);

	return 0;
}

static int ov2311_set_format(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			     struct v4l2_subdev_pad_config *cfg,
#else
			     struct v4l2_subdev_state *sd_state,
#endif
			     struct v4l2_subdev_format *fmt)
{
	struct ov2311 *ov2311 = to_ov2311(sd);
	struct i2c_client *client = v4l2_get_subdevdata(&ov2311->sd);
	const struct ov2311_mode *mode;
	s32 vblank_def;
	s64 hblank;
	int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++)
		if (supported_modes[i].code == fmt->format.code) {
			if (supported_modes[i].width == fmt->format.width
					&& supported_modes[i].height == fmt->format.height) {
				mode = &supported_modes[i];
				break;

			}
		}

	if (i >= ARRAY_SIZE(supported_modes))
		mode = &supported_modes[0];

	mutex_lock(&ov2311->mutex);

	fmt->format.code = supported_formats[0];

	ov2311_update_pad_format(mode, &fmt->format);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		*v4l2_subdev_get_try_format(sd, cfg, fmt->pad) = fmt->format;
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		*v4l2_subdev_state_get_format(sd_state, fmt->pad) = fmt->format;
#endif
	} else {
		ov2311->cur_mode = mode;
		__v4l2_ctrl_s_ctrl(ov2311->link_freq, mode->link_freq_index);
		__v4l2_ctrl_modify_range(ov2311->pixel_rate,
					OV2311_SCLK,
					OV2311_SCLK,
					1,
					OV2311_SCLK);

		hblank = mode->hts - mode->width;
		__v4l2_ctrl_modify_range(ov2311->hblank,
					hblank,
					hblank,
					1,
					hblank);

		/* Update limits and set FPS to default */
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(ov2311->vblank,
					 0,
					 OV2311_VTS_MAX - mode->height, 1,
					 vblank_def);
		__v4l2_ctrl_s_ctrl(ov2311->vblank, vblank_def);
	}

	mutex_unlock(&ov2311->mutex);

	return 0;
}
static int ov2311_get_format(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			     struct v4l2_subdev_pad_config *cfg,
#else
			     struct v4l2_subdev_state *sd_state,
#endif
			     struct v4l2_subdev_format *fmt)
{
	struct ov2311 *ov2311 = to_ov2311(sd);

	mutex_lock(&ov2311->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		fmt->format = *v4l2_subdev_get_try_format(&ov2311->sd, cfg,
							  fmt->pad);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
		fmt->format = *v4l2_subdev_get_try_format(&ov2311->sd, sd_state,
							  fmt->pad);
#else
		fmt->format = *v4l2_subdev_state_get_format(sd_state,
							  fmt->pad);
#endif
	else
		ov2311_update_pad_format(ov2311->cur_mode, &fmt->format);

	mutex_unlock(&ov2311->mutex);

	return 0;
}

static int ov2311_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ov2311 *ov2311 = to_ov2311(sd);

	mutex_lock(&ov2311->mutex);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
	ov2311_update_pad_format(&supported_modes[0],
				 v4l2_subdev_get_try_format(sd, fh->pad, 0));
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
	ov2311_update_pad_format(&supported_modes[0],
				 v4l2_subdev_get_try_format(sd, fh->state, 0));
#else
	ov2311_update_pad_format(&supported_modes[0],
				 v4l2_subdev_state_get_format(fh->state, 0));
#endif
	mutex_unlock(&ov2311->mutex);

	return 0;
}

static const struct v4l2_subdev_video_ops ov2311_video_ops = {
	.s_stream = ov2311_set_stream,
};

static const struct v4l2_subdev_pad_ops ov2311_pad_ops = {
	.set_fmt = ov2311_set_format,
	.get_fmt = ov2311_get_format,
};

static const struct v4l2_subdev_ops ov2311_subdev_ops = {
	.video = &ov2311_video_ops,
	.pad = &ov2311_pad_ops,
};

static const struct media_entity_operations ov2311_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_subdev_internal_ops ov2311_internal_ops = {
	.open = ov2311_open,
};

static int ov2311_identify_module(struct ov2311 *ov2311)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov2311->sd);
	int ret;
	u32 val;
	int retry = 50;

	while (retry--) {
		ret = ov2311_read_reg(ov2311, OV2311_REG_CHIP_ID,
			      OV2311_REG_LEN_08BIT, &val);
		if (ret == 0)
			break;
		usleep_range(100000, 100500);
	}

	if (ret)
		return ret;

	if (val != OV2311_CHIP_ID) {
		dev_err(&client->dev, "read chip id: %x != %x",
			val, OV2311_CHIP_ID);
		return -ENXIO;
	} else {
		dev_err(&client->dev, "read chip id: %x",
			val);
	}

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
static int ov2311_remove(struct i2c_client *client)
#else
static void ov2311_remove(struct i2c_client *client)
#endif
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov2311 *ov2311 = to_ov2311(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	pm_runtime_disable(&client->dev);
	mutex_destroy(&ov2311->mutex);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

static int ov2311_probe(struct i2c_client *client)
{
	struct v4l2_subdev *sd;
	struct ov2311 *ov2311;
	const struct ov2311_reg_list *reg_list;
	int ret;

	ov2311 = devm_kzalloc(&client->dev, sizeof(*ov2311), GFP_KERNEL);
	if (!ov2311)
		return -ENOMEM;

	ov2311->platform_data = client->dev.platform_data;
	if (ov2311->platform_data == NULL) {
		dev_err(&client->dev, "no platform data provided\n");
		return -EINVAL;
	}

	/* initialize subdevice */
	sd = &ov2311->sd;
	v4l2_i2c_subdev_init(sd, client, &ov2311_subdev_ops);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	sd->internal_ops = &ov2311_internal_ops;
	sd->entity.ops = &ov2311_subdev_entity_ops;

	/* initialize subdev media pad */
	ov2311->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &ov2311->pad);
	if (ret < 0) {
		dev_err(&client->dev,
			"%s : media entity init Failed %d\n", __func__, ret);
		return ret;
	}

	ret = ov2311_identify_module(ov2311);
	if (ret) {
		dev_err(&client->dev, "failed to find sensor: %d", ret);
		return ret;
	}

	if (ov2311->platform_data->suffix)
		snprintf(ov2311->sd.name,
				sizeof(ov2311->sd.name), "ov2311 %c",
				ov2311->platform_data->suffix);

	mutex_init(&ov2311->mutex);

	/* 1600x1300 default */
	ov2311->cur_mode = &supported_modes[0];
	ov2311->pre_mode = ov2311->cur_mode;

	reg_list = &ov2311->cur_mode->reg_list;
	ret = ov2311_write_reg_list(ov2311, reg_list);
	if (ret) {
		dev_err(&client->dev, "failed to apply preset mode");
		return ret;
	}

	ret = ov2311_init_controls(ov2311);
	if (ret) {
		dev_err(&client->dev, "failed to init controls: %d", ret);
		goto probe_error_v4l2_ctrl_handler_free;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
	ret = v4l2_async_register_subdev_sensor_common(&ov2311->sd);
#else
	ret = v4l2_async_register_subdev_sensor(&ov2311->sd);
#endif
	if (ret < 0) {
		dev_err(&client->dev, "failed to register V4L2 subdev: %d",
			ret);
		goto probe_error_media_entity_cleanup;
	}

	/*
	 * Device is already turned on by i2c-core with ACPI domain PM.
	 * Enable runtime PM and turn off the device.
	 */
	pm_runtime_set_active(&client->dev);
	pm_runtime_enable(&client->dev);
	pm_runtime_idle(&client->dev);
	dev_err(&client->dev, "Probe Succeeded");

	return 0;

probe_error_media_entity_cleanup:
	media_entity_cleanup(&ov2311->sd.entity);

probe_error_v4l2_ctrl_handler_free:
	v4l2_ctrl_handler_free(ov2311->sd.ctrl_handler);
	mutex_destroy(&ov2311->mutex);
	dev_err(&client->dev, "Probe Failed");

	return ret;
}

static const struct dev_pm_ops ov2311_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ov2311_suspend, ov2311_resume)
};

static const struct i2c_device_id ov2311_id_table[] = {
	{ "ov2311", 0 },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(i2c, ov2311_id_table);

static struct i2c_driver ov2311_i2c_driver = {
	.driver = {
		.name = "ov2311",
		.pm = &ov2311_pm_ops,
	},
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
	.probe_new = ov2311_probe,
#else
	.probe = ov2311_probe,
#endif
	.remove = ov2311_remove,
	.id_table = ov2311_id_table,
};

module_i2c_driver(ov2311_i2c_driver);

MODULE_AUTHOR("He Jiabin <jiabin.he@intel.com>");
MODULE_DESCRIPTION("ov2311 sensor driver");
MODULE_LICENSE("GPL v2");
