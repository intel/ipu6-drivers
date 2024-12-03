// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 Intel Corporation.

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
#include <asm/unaligned.h>
#else
#include <linux/unaligned.h>
#endif
#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
IS_ENABLED(CONFIG_INTEL_VSC)
#include <linux/vsc.h>

static const struct acpi_device_id cvfd_ids[] = {
	{ "INTC1059", 0 },
	{ "INTC1095", 0 },
	{ "INTC100A", 0 },
	{ "INTC10CF", 0 },
	{}
};
#endif

#define OV02C10_LINK_FREQ_400MHZ	400000000ULL
#define OV02C10_SCLK			80000000LL
#define OV02C10_MCLK			19200000
#define OV02C10_DATA_LANES		1
#define OV02C10_RGB_DEPTH		10

#define OV02C10_REG_CHIP_ID		0x300a
#define OV02C10_CHIP_ID			0x560243

#define OV02C10_REG_MODE_SELECT		0x0100
#define OV02C10_MODE_STANDBY		0x00
#define OV02C10_MODE_STREAMING		0x01

/* vertical-timings from sensor */
#define OV02C10_REG_VTS			0x380e
#define OV02C10_VTS_MAX			0xffff

/* Exposure controls from sensor */
#define OV02C10_REG_EXPOSURE		0x3501
#define OV02C10_EXPOSURE_MIN		4
#define OV02C10_EXPOSURE_MAX_MARGIN	8
#define OV02C10_EXPOSURE_STEP		1

/* Analog gain controls from sensor */
#define OV02C10_REG_ANALOG_GAIN		0x3508
#define OV02C10_ANAL_GAIN_MIN		0x10
#define OV02C10_ANAL_GAIN_MAX		0xf8
#define OV02C10_ANAL_GAIN_STEP		1
#define OV02C10_ANAL_GAIN_DEFAULT	0x10

/* Digital gain controls from sensor */
#define OV02C10_REG_DIGILAL_GAIN	0x350a
#define OV02C10_DGTL_GAIN_MIN		0x0400
#define OV02C10_DGTL_GAIN_MAX		0x3fff
#define OV02C10_DGTL_GAIN_STEP		1
#define OV02C10_DGTL_GAIN_DEFAULT	0x0400

/* Rotate */
#define OV02C10_ROTATE_CONTROL			0x3820
#define OV02C10_ISP_X_WIN_CONTROL		0x3811
#define OV02C10_ISP_Y_WIN_CONTROL		0x3813
#define OV02C10_CONFIG_ROTATE			0x18

/* Test Pattern Control */
#define OV02C10_REG_TEST_PATTERN		0x4503
#define OV02C10_TEST_PATTERN_ENABLE	BIT(7)
#define OV02C10_TEST_PATTERN_BAR_SHIFT	0

enum {
	OV02C10_LINK_FREQ_400MHZ_INDEX,
};

enum module_names {
	MODULE_OTHERS = 0,
	MODULE_2BG203N3,
	MODULE_CJFME32,
	MODULE_KBFC645,
};

struct ov02c10_reg {
	u16 address;
	u8 val;
};

struct ov02c10_reg_list {
	u32 num_of_regs;
	const struct ov02c10_reg *regs;
};

struct ov02c10_link_freq_config {
	const struct ov02c10_reg_list reg_list;
};

struct ov02c10_mode {
	/* Frame width in pixels */
	u32 width;

	/* Frame height in pixels */
	u32 height;

	/* Horizontal timining size */
	u32 hts;

	/* Default vertical timining size */
	u32 vts_def;

	/* Min vertical timining size */
	u32 vts_min;

	/* Link frequency needed for this resolution */
	u32 link_freq_index;

	/* MIPI lanes used */
	u8 mipi_lanes;

	/* Sensor register settings for this resolution */
	const struct ov02c10_reg_list reg_list;
};

struct mipi_camera_link_ssdb {
	u8 version;
	u8 sku;
	u8 guid_csi2[16];
	u8 devfunction;
	u8 bus;
	u32 dphylinkenfuses;
	u32 clockdiv;
	u8 link;
	u8 lanes;
	u32 csiparams[10];
	u32 maxlanespeed;
	u8 sensorcalibfileidx;
	u8 sensorcalibfileidxInMBZ[3];
	u8 romtype;
	u8 vcmtype;
	u8 platforminfo;
	u8 platformsubinfo;
	u8 flash;
	u8 privacyled;
	u8 degree;
	u8 mipilinkdefined;
	u32 mclkspeed;
	u8 controllogicid;
	u8 reserved1[3];
	u8 mclkport;
	u8 reserved2[13];
} __packed;

/*
 * 822ace8f-2814-4174-a56b-5f029fe079ee
 * This _DSM GUID returns a string from the sensor device, which acts as a
 * module identifier.
 */
static const guid_t cio2_sensor_module_guid =
	GUID_INIT(0x822ace8f, 0x2814, 0x4174,
		  0xa5, 0x6b, 0x5f, 0x02, 0x9f, 0xe0, 0x79, 0xee);

static const struct ov02c10_reg mipi_data_rate_960mbps[] = {
};

static const struct ov02c10_reg sensor_1928x1092_1lane_30fps_setting[] = {
	{0x0301, 0x08},
	{0x0303, 0x06},
	{0x0304, 0x01},
	{0x0305, 0xe0},
	{0x0313, 0x40},
	{0x031c, 0x4f},
	{0x301b, 0xd2},
	{0x3020, 0x97},
	{0x3022, 0x01},
	{0x3026, 0xb4},
	{0x3027, 0xe1},
	{0x303b, 0x00},
	{0x303c, 0x4f},
	{0x303d, 0xe6},
	{0x303e, 0x00},
	{0x303f, 0x03},
	{0x3021, 0x23},
	{0x3501, 0x04},
	{0x3502, 0x6c},
	{0x3504, 0x0c},
	{0x3507, 0x00},
	{0x3508, 0x08},
	{0x3509, 0x00},
	{0x350a, 0x01},
	{0x350b, 0x00},
	{0x350c, 0x41},
	{0x3600, 0x84},
	{0x3603, 0x08},
	{0x3610, 0x57},
	{0x3611, 0x1b},
	{0x3613, 0x78},
	{0x3623, 0x00},
	{0x3632, 0xa0},
	{0x3642, 0xe8},
	{0x364c, 0x70},
	{0x365f, 0x0f},
	{0x3708, 0x30},
	{0x3714, 0x24},
	{0x3725, 0x02},
	{0x3737, 0x08},
	{0x3739, 0x28},
	{0x3749, 0x32},
	{0x374a, 0x32},
	{0x374b, 0x32},
	{0x374c, 0x32},
	{0x374d, 0x81},
	{0x374e, 0x81},
	{0x374f, 0x81},
	{0x3752, 0x36},
	{0x3753, 0x36},
	{0x3754, 0x36},
	{0x3761, 0x00},
	{0x376c, 0x81},
	{0x3774, 0x18},
	{0x3776, 0x08},
	{0x377c, 0x81},
	{0x377d, 0x81},
	{0x377e, 0x81},
	{0x37a0, 0x44},
	{0x37a6, 0x44},
	{0x37aa, 0x0d},
	{0x37ae, 0x00},
	{0x37cb, 0x03},
	{0x37cc, 0x01},
	{0x37d8, 0x02},
	{0x37d9, 0x10},
	{0x37e1, 0x10},
	{0x37e2, 0x18},
	{0x37e3, 0x08},
	{0x37e4, 0x08},
	{0x37e5, 0x02},
	{0x37e6, 0x08},

	// 1928x1092
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x07},
	{0x3805, 0x8f},
	{0x3806, 0x04},
	{0x3807, 0x47},
	{0x3808, 0x07},
	{0x3809, 0x88},
	{0x380a, 0x04},
	{0x380b, 0x44},
	{0x380c, 0x08},
	{0x380d, 0xe8},
	{0x380e, 0x04},
	{0x380f, 0x8c},
	{0x3810, 0x00},
	{0x3811, 0x03},
	{0x3812, 0x00},
	{0x3813, 0x03},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},

	{0x3820, 0xa8},
	{0x3821, 0x00},
	{0x3822, 0x80},
	{0x3823, 0x08},
	{0x3824, 0x00},
	{0x3825, 0x20},
	{0x3826, 0x00},
	{0x3827, 0x08},
	{0x382a, 0x00},
	{0x382b, 0x08},
	{0x382d, 0x00},
	{0x382e, 0x00},
	{0x382f, 0x23},
	{0x3834, 0x00},
	{0x3839, 0x00},
	{0x383a, 0xd1},
	{0x383e, 0x03},
	{0x393d, 0x29},
	{0x393f, 0x6e},
	{0x394b, 0x06},
	{0x394c, 0x06},
	{0x394d, 0x08},
	{0x394e, 0x0b},
	{0x394f, 0x01},
	{0x3950, 0x01},
	{0x3951, 0x01},
	{0x3952, 0x01},
	{0x3953, 0x01},
	{0x3954, 0x01},
	{0x3955, 0x01},
	{0x3956, 0x01},
	{0x3957, 0x0e},
	{0x3958, 0x08},
	{0x3959, 0x08},
	{0x395a, 0x08},
	{0x395b, 0x13},
	{0x395c, 0x09},
	{0x395d, 0x05},
	{0x395e, 0x02},
	{0x395f, 0x00},
	{0x395f, 0x00},
	{0x3960, 0x00},
	{0x3961, 0x00},
	{0x3962, 0x00},
	{0x3963, 0x00},
	{0x3964, 0x00},
	{0x3965, 0x00},
	{0x3966, 0x00},
	{0x3967, 0x00},
	{0x3968, 0x01},
	{0x3969, 0x01},
	{0x396a, 0x01},
	{0x396b, 0x01},
	{0x396c, 0x10},
	{0x396d, 0xf0},
	{0x396e, 0x11},
	{0x396f, 0x00},
	{0x3970, 0x37},
	{0x3971, 0x37},
	{0x3972, 0x37},
	{0x3973, 0x37},
	{0x3974, 0x00},
	{0x3975, 0x3c},
	{0x3976, 0x3c},
	{0x3977, 0x3c},
	{0x3978, 0x3c},
	{0x3c00, 0x0f},
	{0x3c20, 0x01},
	{0x3c21, 0x08},
	{0x3f00, 0x8b},
	{0x3f02, 0x0f},
	{0x4000, 0xc3},
	{0x4001, 0xe0},
	{0x4002, 0x00},
	{0x4003, 0x40},
	{0x4008, 0x04},
	{0x4009, 0x23},
	{0x400a, 0x04},
	{0x400b, 0x01},
	{0x4077, 0x06},
	{0x4078, 0x00},
	{0x4079, 0x1a},
	{0x407a, 0x7f},
	{0x407b, 0x01},
	{0x4080, 0x03},
	{0x4081, 0x84},
	{0x4308, 0x03},
	{0x4309, 0xff},
	{0x430d, 0x00},
	{0x4806, 0x00},
	{0x4813, 0x00},
	{0x4837, 0x10},
	{0x4857, 0x05},
	{0x4500, 0x07},
	{0x4501, 0x00},
	{0x4503, 0x00},
	{0x450a, 0x04},
	{0x450e, 0x00},
	{0x450f, 0x00},
	{0x4800, 0x24},
	{0x4900, 0x00},
	{0x4901, 0x00},
	{0x4902, 0x01},
	{0x5000, 0xf5},
	{0x5001, 0x50},
	{0x5006, 0x00},
	{0x5080, 0x40},
	{0x5181, 0x2b},
	{0x5202, 0xa3},
	{0x5206, 0x01},
	{0x5207, 0x00},
	{0x520a, 0x01},
	{0x520b, 0x00},
	{0x365d, 0x00},
	{0x4815, 0x40},
	{0x4816, 0x12},
	{0x4f00, 0x01},
	// plls
	{0x0303, 0x05},
	{0x0305, 0x90},
	{0x0316, 0x90},
	{0x3016, 0x12},
};
static const struct ov02c10_reg sensor_1928x1092_2lane_30fps_setting[] = {
	{0x0301, 0x08},
	{0x0303, 0x06},
	{0x0304, 0x01},
	{0x0305, 0xe0},
	{0x0313, 0x40},
	{0x031c, 0x4f},
	{0x301b, 0xf0},
	{0x3020, 0x97},
	{0x3022, 0x01},
	{0x3026, 0xb4},
	{0x3027, 0xf1},
	{0x303b, 0x00},
	{0x303c, 0x4f},
	{0x303d, 0xe6},
	{0x303e, 0x00},
	{0x303f, 0x03},
	{0x3021, 0x23},
	{0x3501, 0x04},
	{0x3502, 0x6c},
	{0x3504, 0x0c},
	{0x3507, 0x00},
	{0x3508, 0x08},
	{0x3509, 0x00},
	{0x350a, 0x01},
	{0x350b, 0x00},
	{0x350c, 0x41},
	{0x3600, 0x84},
	{0x3603, 0x08},
	{0x3610, 0x57},
	{0x3611, 0x1b},
	{0x3613, 0x78},
	{0x3623, 0x00},
	{0x3632, 0xa0},
	{0x3642, 0xe8},
	{0x364c, 0x70},
	{0x365f, 0x0f},
	{0x3708, 0x30},
	{0x3714, 0x24},
	{0x3725, 0x02},
	{0x3737, 0x08},
	{0x3739, 0x28},
	{0x3749, 0x32},
	{0x374a, 0x32},
	{0x374b, 0x32},
	{0x374c, 0x32},
	{0x374d, 0x81},
	{0x374e, 0x81},
	{0x374f, 0x81},
	{0x3752, 0x36},
	{0x3753, 0x36},
	{0x3754, 0x36},
	{0x3761, 0x00},
	{0x376c, 0x81},
	{0x3774, 0x18},
	{0x3776, 0x08},
	{0x377c, 0x81},
	{0x377d, 0x81},
	{0x377e, 0x81},
	{0x37a0, 0x44},
	{0x37a6, 0x44},
	{0x37aa, 0x0d},
	{0x37ae, 0x00},
	{0x37cb, 0x03},
	{0x37cc, 0x01},
	{0x37d8, 0x02},
	{0x37d9, 0x10},
	{0x37e1, 0x10},
	{0x37e2, 0x18},
	{0x37e3, 0x08},
	{0x37e4, 0x08},
	{0x37e5, 0x02},
	{0x37e6, 0x08},

	// 1928x1092
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x07},
	{0x3805, 0x8f},
	{0x3806, 0x04},
	{0x3807, 0x47},
	{0x3808, 0x07},
	{0x3809, 0x88},
	{0x380a, 0x04},
	{0x380b, 0x44},
	{0x380c, 0x04},
	{0x380d, 0x74},
	{0x380e, 0x09},
	{0x380f, 0x18},
	{0x3810, 0x00},
	{0x3811, 0x03},
	{0x3812, 0x00},
	{0x3813, 0x03},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},

	{0x3820, 0xa8},
	{0x3821, 0x00},
	{0x3822, 0x80},
	{0x3823, 0x08},
	{0x3824, 0x00},
	{0x3825, 0x20},
	{0x3826, 0x00},
	{0x3827, 0x08},
	{0x382a, 0x00},
	{0x382b, 0x08},
	{0x382d, 0x00},
	{0x382e, 0x00},
	{0x382f, 0x23},
	{0x3834, 0x00},
	{0x3839, 0x00},
	{0x383a, 0xd1},
	{0x383e, 0x03},
	{0x393d, 0x29},
	{0x393f, 0x6e},
	{0x394b, 0x06},
	{0x394c, 0x06},
	{0x394d, 0x08},
	{0x394e, 0x0a},
	{0x394f, 0x01},
	{0x3950, 0x01},
	{0x3951, 0x01},
	{0x3952, 0x01},
	{0x3953, 0x01},
	{0x3954, 0x01},
	{0x3955, 0x01},
	{0x3956, 0x01},
	{0x3957, 0x0e},
	{0x3958, 0x08},
	{0x3959, 0x08},
	{0x395a, 0x08},
	{0x395b, 0x13},
	{0x395c, 0x09},
	{0x395d, 0x05},
	{0x395e, 0x02},
	{0x395f, 0x00},
	{0x395f, 0x00},
	{0x3960, 0x00},
	{0x3961, 0x00},
	{0x3962, 0x00},
	{0x3963, 0x00},
	{0x3964, 0x00},
	{0x3965, 0x00},
	{0x3966, 0x00},
	{0x3967, 0x00},
	{0x3968, 0x01},
	{0x3969, 0x01},
	{0x396a, 0x01},
	{0x396b, 0x01},
	{0x396c, 0x10},
	{0x396d, 0xf0},
	{0x396e, 0x11},
	{0x396f, 0x00},
	{0x3970, 0x37},
	{0x3971, 0x37},
	{0x3972, 0x37},
	{0x3973, 0x37},
	{0x3974, 0x00},
	{0x3975, 0x3c},
	{0x3976, 0x3c},
	{0x3977, 0x3c},
	{0x3978, 0x3c},
	{0x3c00, 0x0f},
	{0x3c20, 0x01},
	{0x3c21, 0x08},
	{0x3f00, 0x8b},
	{0x3f02, 0x0f},
	{0x4000, 0xc3},
	{0x4001, 0xe0},
	{0x4002, 0x00},
	{0x4003, 0x40},
	{0x4008, 0x04},
	{0x4009, 0x23},
	{0x400a, 0x04},
	{0x400b, 0x01},
	{0x4041, 0x20},
	{0x4077, 0x06},
	{0x4078, 0x00},
	{0x4079, 0x1a},
	{0x407a, 0x7f},
	{0x407b, 0x01},
	{0x4080, 0x03},
	{0x4081, 0x84},
	{0x4308, 0x03},
	{0x4309, 0xff},
	{0x430d, 0x00},
	{0x4806, 0x00},
	{0x4813, 0x00},
	{0x4837, 0x10},
	{0x4857, 0x05},
	{0x4884, 0x04},
	{0x4500, 0x07},
	{0x4501, 0x00},
	{0x4503, 0x00},
	{0x450a, 0x04},
	{0x450e, 0x00},
	{0x450f, 0x00},
	{0x4800, 0x64},
	{0x4900, 0x00},
	{0x4901, 0x00},
	{0x4902, 0x01},
	{0x4d00, 0x03},
	{0x4d01, 0xd8},
	{0x4d02, 0xba},
	{0x4d03, 0xa0},
	{0x4d04, 0xb7},
	{0x4d05, 0x34},
	{0x4d0d, 0x00},
	{0x5000, 0xfd},
	{0x5001, 0x50},
	{0x5006, 0x00},
	{0x5080, 0x40},
	{0x5181, 0x2b},
	{0x5202, 0xa3},
	{0x5206, 0x01},
	{0x5207, 0x00},
	{0x520a, 0x01},
	{0x520b, 0x00},
	{0x365d, 0x00},
	{0x4815, 0x40},
	{0x4816, 0x12},
	{0x481f, 0x30},
	{0x4f00, 0x01},
	// plls
	{0x0303, 0x05},
	{0x0305, 0x90},
	{0x0316, 0x90},
	{0x3016, 0x32},
};

static const char * const ov02c10_test_pattern_menu[] = {
	"Disabled",
	"Color Bar",
	"Top-Bottom Darker Color Bar",
	"Right-Left Darker Color Bar",
	"Color Bar type 4",
};

static const char * const ov02c10_module_names[] = {
	[MODULE_OTHERS] = "",
	[MODULE_2BG203N3] = "2BG203N3",
	[MODULE_CJFME32] = "CJFME32",
	[MODULE_KBFC645] = "KBFC645",
};

static const s64 link_freq_menu_items[] = {
	OV02C10_LINK_FREQ_400MHZ,
};

static const struct ov02c10_link_freq_config link_freq_configs[] = {
	[OV02C10_LINK_FREQ_400MHZ_INDEX] = {
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mipi_data_rate_960mbps),
			.regs = mipi_data_rate_960mbps,
		}
	},
};

static const struct ov02c10_mode supported_modes[] = {
	{
		.width = 1928,
		.height = 1092,
		.hts = 2280,
		.vts_def = 1164,
		.vts_min = 1164,
		.mipi_lanes = 1,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(sensor_1928x1092_1lane_30fps_setting),
			.regs = sensor_1928x1092_1lane_30fps_setting,
		},
		.link_freq_index = OV02C10_LINK_FREQ_400MHZ_INDEX,
	},
	{
		.width = 1928,
		.height = 1092,
		.hts = 1140,
		.vts_def = 2328,
		.vts_min = 2328,
		.mipi_lanes = 2,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(sensor_1928x1092_2lane_30fps_setting),
			.regs = sensor_1928x1092_2lane_30fps_setting,
		},
		.link_freq_index = OV02C10_LINK_FREQ_400MHZ_INDEX,
	},
};

struct ov02c10 {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;

	/* V4L2 Controls */
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *exposure;

	struct clk *img_clk;
	struct regulator *avdd;
	struct gpio_desc *reset;
	struct gpio_desc *handshake;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
IS_ENABLED(CONFIG_INTEL_VSC)
	struct vsc_mipi_config conf;
	struct vsc_camera_status status;
	struct v4l2_ctrl *privacy_status;
#endif
	/* Current mode */
	const struct ov02c10_mode *cur_mode;

	/* To serialize asynchronus callbacks */
	struct mutex mutex;

	/* MIPI lanes used */
	u8 mipi_lanes;

	/* Streaming on/off */
	bool streaming;

	/* Module name index */
	u8 module_name_index;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
IS_ENABLED(CONFIG_INTEL_VSC)

	bool use_intel_vsc;
#endif
};

static inline struct ov02c10 *to_ov02c10(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct ov02c10, sd);
}

static int ov02c10_read_reg(struct ov02c10 *ov02c10, u16 reg, u16 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov02c10->sd);
	struct i2c_msg msgs[2];
	u8 addr_buf[2];
	u8 data_buf[4] = {0};
	int ret = 0;

	if (len > sizeof(data_buf))
		return -EINVAL;

	put_unaligned_be16(reg, addr_buf);
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(addr_buf);
	msgs[0].buf = addr_buf;
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_buf[sizeof(data_buf) - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));

	if (ret != ARRAY_SIZE(msgs))
		return ret < 0 ? ret : -EIO;

	*val = get_unaligned_be32(data_buf);

	return 0;
}

static int ov02c10_write_reg(struct ov02c10 *ov02c10, u16 reg, u16 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov02c10->sd);
	u8 buf[6];
	int ret = 0;

	if (len > 4)
		return -EINVAL;

	put_unaligned_be16(reg, buf);
	put_unaligned_be32(val << 8 * (4 - len), buf + 2);

	ret = i2c_master_send(client, buf, len + 2);
	if (ret != len + 2)
		return ret < 0 ? ret : -EIO;

	return 0;
}

static int ov02c10_write_reg_list(struct ov02c10 *ov02c10,
				  const struct ov02c10_reg_list *r_list)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov02c10->sd);
	unsigned int i;
	int ret = 0;

	for (i = 0; i < r_list->num_of_regs; i++) {
		ret = ov02c10_write_reg(ov02c10, r_list->regs[i].address, 1,
					r_list->regs[i].val);
		if (ret) {
			dev_err_ratelimited(&client->dev,
					    "write reg 0x%4.4x return err = %d",
					    r_list->regs[i].address, ret);
			return ret;
		}
	}

	return 0;
}

static int ov02c10_test_pattern(struct ov02c10 *ov02c10, u32 pattern)
{
	if (pattern)
		pattern = (pattern - 1) << OV02C10_TEST_PATTERN_BAR_SHIFT |
			  OV02C10_TEST_PATTERN_ENABLE;

	return ov02c10_write_reg(ov02c10, OV02C10_REG_TEST_PATTERN, 1, pattern);
}

static int ov02c10_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov02c10 *ov02c10 = container_of(ctrl->handler,
					     struct ov02c10, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&ov02c10->sd);
	s64 exposure_max;
	int ret = 0;

	/* Propagate change of current control to all related controls */
	if (ctrl->id == V4L2_CID_VBLANK) {
		/* Update max exposure while meeting expected vblanking */
		exposure_max = ov02c10->cur_mode->height + ctrl->val -
			       OV02C10_EXPOSURE_MAX_MARGIN;
		__v4l2_ctrl_modify_range(ov02c10->exposure,
					 ov02c10->exposure->minimum,
					 exposure_max, ov02c10->exposure->step,
					 exposure_max);
	}

	/* V4L2 controls values will be applied only when power is already up */
	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		ret = ov02c10_write_reg(ov02c10, OV02C10_REG_ANALOG_GAIN, 2,
					ctrl->val << 4);
		break;

	case V4L2_CID_DIGITAL_GAIN:
		ret = ov02c10_write_reg(ov02c10, OV02C10_REG_DIGILAL_GAIN, 3,
					ctrl->val << 6);
		break;

	case V4L2_CID_EXPOSURE:
		ret = ov02c10_write_reg(ov02c10, OV02C10_REG_EXPOSURE, 2,
					ctrl->val);
		break;

	case V4L2_CID_VBLANK:
		ret = ov02c10_write_reg(ov02c10, OV02C10_REG_VTS, 2,
					ov02c10->cur_mode->height + ctrl->val);
		break;

	case V4L2_CID_TEST_PATTERN:
		ret = ov02c10_test_pattern(ov02c10, ctrl->val);
		break;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
IS_ENABLED(CONFIG_INTEL_VSC)
	case V4L2_CID_PRIVACY:
		dev_dbg(&client->dev, "set privacy to %d", ctrl->val);
		break;
#endif

	default:
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops ov02c10_ctrl_ops = {
	.s_ctrl = ov02c10_set_ctrl,
};

static int ov02c10_init_controls(struct ov02c10 *ov02c10)
{
	struct v4l2_ctrl_handler *ctrl_hdlr;
	const struct ov02c10_mode *cur_mode;
	s64 exposure_max, h_blank;
	u32 vblank_min, vblank_max, vblank_default;
	int size;
	int ret = 0;

	ctrl_hdlr = &ov02c10->ctrl_handler;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
IS_ENABLED(CONFIG_INTEL_VSC)
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 9);
#else
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 8);
#endif
	if (ret)
		return ret;

	ctrl_hdlr->lock = &ov02c10->mutex;
	cur_mode = ov02c10->cur_mode;
	size = ARRAY_SIZE(link_freq_menu_items);

	ov02c10->link_freq = v4l2_ctrl_new_int_menu(ctrl_hdlr,
						    &ov02c10_ctrl_ops,
						    V4L2_CID_LINK_FREQ,
						    size - 1, 0,
						    link_freq_menu_items);
	if (ov02c10->link_freq)
		ov02c10->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	ov02c10->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &ov02c10_ctrl_ops,
						V4L2_CID_PIXEL_RATE, 0,
						OV02C10_SCLK, 1, OV02C10_SCLK);

	vblank_min = cur_mode->vts_min - cur_mode->height;
	vblank_max = OV02C10_VTS_MAX - cur_mode->height;
	vblank_default = cur_mode->vts_def - cur_mode->height;
	ov02c10->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &ov02c10_ctrl_ops,
					    V4L2_CID_VBLANK, vblank_min,
					    vblank_max, 1, vblank_default);

	h_blank = cur_mode->hts - cur_mode->width;
	ov02c10->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &ov02c10_ctrl_ops,
					    V4L2_CID_HBLANK, h_blank, h_blank,
					    1, h_blank);
	if (ov02c10->hblank)
		ov02c10->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
IS_ENABLED(CONFIG_INTEL_VSC)
	ov02c10->privacy_status = v4l2_ctrl_new_std(ctrl_hdlr,
						    &ov02c10_ctrl_ops,
						    V4L2_CID_PRIVACY, 0, 1, 1,
						    !(ov02c10->status.status));
#endif

	v4l2_ctrl_new_std(ctrl_hdlr, &ov02c10_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  OV02C10_ANAL_GAIN_MIN, OV02C10_ANAL_GAIN_MAX,
			  OV02C10_ANAL_GAIN_STEP, OV02C10_ANAL_GAIN_DEFAULT);
	v4l2_ctrl_new_std(ctrl_hdlr, &ov02c10_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  OV02C10_DGTL_GAIN_MIN, OV02C10_DGTL_GAIN_MAX,
			  OV02C10_DGTL_GAIN_STEP, OV02C10_DGTL_GAIN_DEFAULT);
	exposure_max = cur_mode->vts_def - OV02C10_EXPOSURE_MAX_MARGIN;
	ov02c10->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &ov02c10_ctrl_ops,
					      V4L2_CID_EXPOSURE,
					      OV02C10_EXPOSURE_MIN,
					      exposure_max,
					      OV02C10_EXPOSURE_STEP,
					      exposure_max);
	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &ov02c10_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(ov02c10_test_pattern_menu) - 1,
				     0, 0, ov02c10_test_pattern_menu);
	if (ctrl_hdlr->error)
		return ctrl_hdlr->error;

	ov02c10->sd.ctrl_handler = ctrl_hdlr;

	return 0;
}

static void ov02c10_update_pad_format(const struct ov02c10_mode *mode,
				      struct v4l2_mbus_framefmt *fmt)
{
	fmt->width = mode->width;
	fmt->height = mode->height;
	fmt->code = MEDIA_BUS_FMT_SGRBG10_1X10;
	fmt->field = V4L2_FIELD_NONE;
}

static int ov02c10_start_streaming(struct ov02c10 *ov02c10)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov02c10->sd);
	const struct ov02c10_reg_list *reg_list;
	int link_freq_index;
	int ret = 0;
	u32 rotate, shift_x, shift_y;

	link_freq_index = ov02c10->cur_mode->link_freq_index;
	reg_list = &link_freq_configs[link_freq_index].reg_list;
	ret = ov02c10_write_reg_list(ov02c10, reg_list);
	if (ret) {
		dev_err(&client->dev, "failed to set plls");
		return ret;
	}

	reg_list = &ov02c10->cur_mode->reg_list;
	ret = ov02c10_write_reg_list(ov02c10, reg_list);
	if (ret) {
		dev_err(&client->dev, "failed to set mode");
		return ret;
	}

	ret = __v4l2_ctrl_handler_setup(ov02c10->sd.ctrl_handler);
	if (ret)
		return ret;

	switch (ov02c10->module_name_index) {
	case MODULE_CJFME32:
	case MODULE_2BG203N3:
	case MODULE_KBFC645:
		ret = ov02c10_read_reg(ov02c10, OV02C10_ROTATE_CONTROL,
				       1, &rotate);
		if (ret)
			dev_err(&client->dev,
				"read ROTATE_CONTROL fail: %d", ret);

		ret = ov02c10_read_reg(ov02c10, OV02C10_ISP_X_WIN_CONTROL,
				       1, &shift_x);
		if (ret)
			dev_err(&client->dev,
				"read ISP_X_WIN_CONTROL fail: %d", ret);

		ret = ov02c10_read_reg(ov02c10, OV02C10_ISP_Y_WIN_CONTROL,
				       1, &shift_y);
		if (ret)
			dev_err(&client->dev,
				"read ISP_Y_WIN_CONTROL fail: %d", ret);

		rotate ^= OV02C10_CONFIG_ROTATE;
		shift_x = shift_x - 1;
		shift_y = shift_y - 1;

		ret = ov02c10_write_reg(ov02c10, OV02C10_ROTATE_CONTROL,
					1, rotate);
		if (ret)
			dev_err(&client->dev,
				"write ROTATE_CONTROL fail: %d", ret);

		ret = ov02c10_write_reg(ov02c10, OV02C10_ISP_X_WIN_CONTROL,
					1, shift_x);
		if (ret)
			dev_err(&client->dev,
				"write ISP_X_WIN_CONTROL fail: %d", ret);

		ret = ov02c10_write_reg(ov02c10, OV02C10_ISP_Y_WIN_CONTROL,
					1, shift_y);
		if (ret)
			dev_err(&client->dev,
				"write ISP_Y_WIN_CONTROL fail: %d", ret);
		break;
	}

	ret = ov02c10_write_reg(ov02c10, OV02C10_REG_MODE_SELECT, 1,
				OV02C10_MODE_STREAMING);
	if (ret)
		dev_err(&client->dev, "failed to start streaming");

	return ret;
}

static void ov02c10_stop_streaming(struct ov02c10 *ov02c10)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov02c10->sd);
	int ret = 0;

	ret = ov02c10_write_reg(ov02c10, OV02C10_REG_MODE_SELECT, 1,
				OV02C10_MODE_STANDBY);
	if (ret)
		dev_err(&client->dev, "failed to stop streaming");
}

static int ov02c10_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov02c10 *ov02c10 = to_ov02c10(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (ov02c10->streaming == enable)
		return 0;

	mutex_lock(&ov02c10->mutex);
	if (enable) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			mutex_unlock(&ov02c10->mutex);
			return ret;
		}

		ret = ov02c10_start_streaming(ov02c10);
		if (ret) {
			enable = 0;
			ov02c10_stop_streaming(ov02c10);
			pm_runtime_put(&client->dev);
		}
	} else {
		ov02c10_stop_streaming(ov02c10);
		pm_runtime_put(&client->dev);
	}

	ov02c10->streaming = enable;
	mutex_unlock(&ov02c10->mutex);

	return ret;
}

/* This function tries to get power control resources */
static int ov02c10_get_pm_resources(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ov02c10 *ov02c10 = to_ov02c10(sd);
	int ret;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
IS_ENABLED(CONFIG_INTEL_VSC)
	acpi_handle handle = ACPI_HANDLE(dev);
	struct acpi_handle_list deps;
	acpi_status status;
	int i = 0;

	ov02c10->use_intel_vsc = false;
	if (!acpi_has_method(handle, "_DEP"))
		return false;

	status = acpi_evaluate_reference(handle, "_DEP", NULL, &deps);
	if (ACPI_FAILURE(status)) {
		acpi_handle_debug(handle, "Failed to evaluate _DEP.\n");
		return false;
	}
	for (i = 0; i < deps.count; i++) {
		struct acpi_device *dep = NULL;

		if (deps.handles[i])
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
			acpi_bus_get_device(deps.handles[i], &dep);
#else
			dep = acpi_fetch_acpi_dev(deps.handles[i]);
#endif

		if (dep && !acpi_match_device_ids(dep, cvfd_ids)) {
			ov02c10->use_intel_vsc = true;
			return 0;
		}
	}
#endif
	ov02c10->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ov02c10->reset))
		return dev_err_probe(dev, PTR_ERR(ov02c10->reset),
				     "failed to get reset gpio\n");

	ov02c10->handshake = devm_gpiod_get_optional(dev, "handshake",
						   GPIOD_OUT_LOW);
	if (IS_ERR(ov02c10->handshake))
		return dev_err_probe(dev, PTR_ERR(ov02c10->handshake),
				     "failed to get handshake gpio\n");

	ov02c10->img_clk = devm_clk_get_optional(dev, NULL);
	if (IS_ERR(ov02c10->img_clk))
		return dev_err_probe(dev, PTR_ERR(ov02c10->img_clk),
				     "failed to get imaging clock\n");

	ov02c10->avdd = devm_regulator_get_optional(dev, "avdd");
	if (IS_ERR(ov02c10->avdd)) {
		ret = PTR_ERR(ov02c10->avdd);
		ov02c10->avdd = NULL;
		if (ret != -ENODEV)
			return dev_err_probe(dev, ret,
					     "failed to get avdd regulator\n");
	}

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
IS_ENABLED(CONFIG_INTEL_VSC)
static void ov02c10_vsc_privacy_callback(void *handle,
				       enum vsc_privacy_status status)
{
	struct ov02c10 *ov02c10 = handle;

	v4l2_ctrl_s_ctrl(ov02c10->privacy_status, !status);
}
#endif

static int ov02c10_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ov02c10 *ov02c10 = to_ov02c10(sd);
	int ret = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
IS_ENABLED(CONFIG_INTEL_VSC)
	if (ov02c10->use_intel_vsc) {
		ret = vsc_release_camera_sensor(&ov02c10->status);
		if (ret && ret != -EAGAIN)
			dev_err(dev, "Release VSC failed");

		return ret;
	}
#endif
	gpiod_set_value_cansleep(ov02c10->reset, 1);
	gpiod_set_value_cansleep(ov02c10->handshake, 0);

	if (ov02c10->avdd)
		ret = regulator_disable(ov02c10->avdd);

	clk_disable_unprepare(ov02c10->img_clk);

	return ret;
}

static int ov02c10_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ov02c10 *ov02c10 = to_ov02c10(sd);
	int ret;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
IS_ENABLED(CONFIG_INTEL_VSC)
	if (ov02c10->use_intel_vsc) {
		ov02c10->conf.lane_num = ov02c10->mipi_lanes;
		/* frequency unit 100k */
		ov02c10->conf.freq = OV02C10_LINK_FREQ_400MHZ / 100000;
		ret = vsc_acquire_camera_sensor(&ov02c10->conf,
						ov02c10_vsc_privacy_callback,
						ov02c10, &ov02c10->status);
		if (ret == -EAGAIN)
			return -EPROBE_DEFER;
		if (ret) {
			dev_err(dev, "Acquire VSC failed");
			return ret;
		}
		if (ov02c10->privacy_status)
			__v4l2_ctrl_s_ctrl(ov02c10->privacy_status,
					   !(ov02c10->status.status));

		return ret;
	}
#endif
	ret = clk_prepare_enable(ov02c10->img_clk);
	if (ret < 0) {
		dev_err(dev, "failed to enable imaging clock: %d", ret);
		return ret;
	}

	if (ov02c10->avdd) {
		ret = regulator_enable(ov02c10->avdd);
		if (ret < 0) {
			dev_err(dev, "failed to enable avdd: %d", ret);
			clk_disable_unprepare(ov02c10->img_clk);
			return ret;
		}
	}
	gpiod_set_value_cansleep(ov02c10->handshake, 1);
	gpiod_set_value_cansleep(ov02c10->reset, 0);

	/* Lattice MIPI aggregator with some version FW needs longer delay
	   after handshake triggered. We set 25ms as a safe value and wait
	   for a stable version FW. */
	msleep_interruptible(25);

	return ret;
}

static int __maybe_unused ov02c10_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov02c10 *ov02c10 = to_ov02c10(sd);

	mutex_lock(&ov02c10->mutex);
	if (ov02c10->streaming)
		ov02c10_stop_streaming(ov02c10);

	mutex_unlock(&ov02c10->mutex);

	return 0;
}

static int __maybe_unused ov02c10_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov02c10 *ov02c10 = to_ov02c10(sd);
	int ret = 0;

	mutex_lock(&ov02c10->mutex);
	if (!ov02c10->streaming)
		goto exit;

	ret = ov02c10_start_streaming(ov02c10);
	if (ret) {
		ov02c10->streaming = false;
		ov02c10_stop_streaming(ov02c10);
	}

exit:
	mutex_unlock(&ov02c10->mutex);
	return ret;
}

static int ov02c10_set_format(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			      struct v4l2_subdev_pad_config *cfg,
#else
			      struct v4l2_subdev_state *sd_state,
#endif
			      struct v4l2_subdev_format *fmt)
{
	struct ov02c10 *ov02c10 = to_ov02c10(sd);
	const struct ov02c10_mode *mode;
	s32 vblank_def, h_blank;

	if (ov02c10->mipi_lanes == 1)
		mode = &supported_modes[0];
	else if (ov02c10->mipi_lanes == 2)
		mode = &supported_modes[1];
	else {
		mode = v4l2_find_nearest_size(supported_modes,
					      ARRAY_SIZE(supported_modes),
					      width, height, fmt->format.width,
					      fmt->format.height);
	}

	mutex_lock(&ov02c10->mutex);
	ov02c10_update_pad_format(mode, &fmt->format);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		*v4l2_subdev_get_try_format(sd, cfg, fmt->pad) = fmt->format;
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		*v4l2_subdev_state_get_format(sd_state, fmt->pad) = fmt->format;
#endif
	} else {
		ov02c10->cur_mode = mode;
		__v4l2_ctrl_s_ctrl(ov02c10->link_freq, mode->link_freq_index);
		__v4l2_ctrl_s_ctrl_int64(ov02c10->pixel_rate, OV02C10_SCLK);

		/* Update limits and set FPS to default */
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(ov02c10->vblank,
					 mode->vts_min - mode->height,
					 OV02C10_VTS_MAX - mode->height, 1,
					 vblank_def);
		__v4l2_ctrl_s_ctrl(ov02c10->vblank, vblank_def);
		h_blank = mode->hts - mode->width;
		__v4l2_ctrl_modify_range(ov02c10->hblank, h_blank, h_blank, 1,
					 h_blank);
	}
	mutex_unlock(&ov02c10->mutex);

	return 0;
}

static int ov02c10_get_format(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			      struct v4l2_subdev_pad_config *cfg,
#else
			      struct v4l2_subdev_state *sd_state,
#endif
			      struct v4l2_subdev_format *fmt)
{
	struct ov02c10 *ov02c10 = to_ov02c10(sd);

	mutex_lock(&ov02c10->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		fmt->format = *v4l2_subdev_get_try_format(&ov02c10->sd, cfg,
							  fmt->pad);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
		fmt->format = *v4l2_subdev_get_try_format(&ov02c10->sd,
							  sd_state, fmt->pad);
#else
		fmt->format = *v4l2_subdev_state_get_format(
							  sd_state, fmt->pad);
#endif
	else
		ov02c10_update_pad_format(ov02c10->cur_mode, &fmt->format);

	mutex_unlock(&ov02c10->mutex);

	return 0;
}

static int ov02c10_enum_mbus_code(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
				  struct v4l2_subdev_pad_config *cfg,
#else
				  struct v4l2_subdev_state *sd_state,
#endif
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index > 0)
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_SGRBG10_1X10;

	return 0;
}

static int ov02c10_enum_frame_size(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
				   struct v4l2_subdev_pad_config *cfg,
#else
				   struct v4l2_subdev_state *sd_state,
#endif
				   struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != MEDIA_BUS_FMT_SGRBG10_1X10)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = supported_modes[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

static int ov02c10_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ov02c10 *ov02c10 = to_ov02c10(sd);

	mutex_lock(&ov02c10->mutex);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
	ov02c10_update_pad_format(&supported_modes[0],
				  v4l2_subdev_get_try_format(sd, fh->pad, 0));
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
	ov02c10_update_pad_format(&supported_modes[0],
				  v4l2_subdev_get_try_format(sd, fh->state, 0));
#else
	ov02c10_update_pad_format(&supported_modes[0],
				  v4l2_subdev_state_get_format(fh->state, 0));
#endif
	mutex_unlock(&ov02c10->mutex);

	return 0;
}

static const struct v4l2_subdev_video_ops ov02c10_video_ops = {
	.s_stream = ov02c10_set_stream,
};

static const struct v4l2_subdev_pad_ops ov02c10_pad_ops = {
	.set_fmt = ov02c10_set_format,
	.get_fmt = ov02c10_get_format,
	.enum_mbus_code = ov02c10_enum_mbus_code,
	.enum_frame_size = ov02c10_enum_frame_size,
};

static const struct v4l2_subdev_ops ov02c10_subdev_ops = {
	.video = &ov02c10_video_ops,
	.pad = &ov02c10_pad_ops,
};

static const struct media_entity_operations ov02c10_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_subdev_internal_ops ov02c10_internal_ops = {
	.open = ov02c10_open,
};

static void ov02c10_read_mipi_lanes(struct ov02c10 *ov02c10)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov02c10->sd);
	struct mipi_camera_link_ssdb ssdb;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	struct acpi_device *adev = ACPI_COMPANION(&client->dev);
	union acpi_object *obj;
	acpi_status status;

	if (!adev) {
		dev_info(&client->dev, "Not ACPI device\n");
		return;
	}
	status = acpi_evaluate_object(adev->handle, "SSDB", NULL, &buffer);
	if (ACPI_FAILURE(status)) {
		dev_info(&client->dev, "ACPI fail: %d\n", -ENODEV);
		return;
	}

	obj = buffer.pointer;
	if (!obj) {
		dev_info(&client->dev, "Couldn't locate ACPI buffer\n");
		return;
	}

	if (obj->type != ACPI_TYPE_BUFFER) {
		dev_info(&client->dev, "Not an ACPI buffer\n");
		goto out_free_buff;
	}

	if (obj->buffer.length > sizeof(ssdb)) {
		dev_err(&client->dev, "Given buffer is too small\n");
		goto out_free_buff;
	}
	memcpy(&ssdb, obj->buffer.pointer, obj->buffer.length);
	ov02c10->mipi_lanes = ssdb.lanes;

out_free_buff:
	kfree(buffer.pointer);
}

static int ov02c10_identify_module(struct ov02c10 *ov02c10)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov02c10->sd);
	int ret;
	u32 val;

	ret = ov02c10_read_reg(ov02c10, OV02C10_REG_CHIP_ID, 3, &val);
	if (ret)
		return ret;

	if (val == 0)
		return -EPROBE_DEFER;

	if (val != OV02C10_CHIP_ID) {
		dev_err(&client->dev, "chip id mismatch: %x!=%x",
			OV02C10_CHIP_ID, val);
		return -ENXIO;
	}

	return 0;
}

static int ov02c10_check_hwcfg(struct device *dev)
{
	struct v4l2_fwnode_endpoint bus_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	struct fwnode_handle *ep;
	struct fwnode_handle *fwnode = dev_fwnode(dev);
	unsigned int i, j;
	int ret;
	u32 ext_clk;

	if (!fwnode)
		return -ENXIO;

	ep = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (!ep)
		return -EPROBE_DEFER;

	ret = fwnode_property_read_u32(dev_fwnode(dev), "clock-frequency",
				       &ext_clk);
	if (ret) {
		dev_err(dev, "can't get clock frequency");
		return ret;
	}

	ret = v4l2_fwnode_endpoint_alloc_parse(ep, &bus_cfg);
	fwnode_handle_put(ep);
	if (ret)
		return ret;

	if (!bus_cfg.nr_of_link_frequencies) {
		dev_err(dev, "no link frequencies defined");
		ret = -EINVAL;
		goto out_err;
	}

	for (i = 0; i < ARRAY_SIZE(link_freq_menu_items); i++) {
		for (j = 0; j < bus_cfg.nr_of_link_frequencies; j++) {
			if (link_freq_menu_items[i] ==
				bus_cfg.link_frequencies[j])
				break;
		}

		if (j == bus_cfg.nr_of_link_frequencies) {
			dev_err(dev, "no link frequency %lld supported",
				link_freq_menu_items[i]);
			ret = -EINVAL;
			goto out_err;
		}
	}

out_err:
	v4l2_fwnode_endpoint_free(&bus_cfg);

	return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
static int ov02c10_remove(struct i2c_client *client)
#else
static void ov02c10_remove(struct i2c_client *client)
#endif
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov02c10 *ov02c10 = to_ov02c10(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);
	pm_runtime_disable(&client->dev);
	mutex_destroy(&ov02c10->mutex);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

static int ov02c10_read_module_name(struct ov02c10 *ov02c10)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov02c10->sd);
	struct device *dev = &client->dev;
	union acpi_object *obj;
	struct acpi_device *adev = ACPI_COMPANION(dev);
	int i;

	ov02c10->module_name_index = 0;
	if (!adev)
		return 0;

	obj = acpi_evaluate_dsm_typed(adev->handle,
				      &cio2_sensor_module_guid, 0x00,
				      0x01, NULL, ACPI_TYPE_STRING);
	if (!obj)
		return 0;

	dev_dbg(dev, "module name: %s", obj->string.pointer);
	for (i = 1; i < ARRAY_SIZE(ov02c10_module_names); i++) {
		if (!strcmp(ov02c10_module_names[i], obj->string.pointer)) {
			ov02c10->module_name_index = i;
			break;
		}
	}
	ACPI_FREE(obj);

	return 0;
}

static int ov02c10_probe(struct i2c_client *client)
{
	struct ov02c10 *ov02c10;
	int ret = 0;

	/* Check HW config */
	ret = ov02c10_check_hwcfg(&client->dev);
	if (ret) {
		dev_err(&client->dev, "failed to check hwcfg: %d", ret);
		return ret;
	}

	ov02c10 = devm_kzalloc(&client->dev, sizeof(*ov02c10), GFP_KERNEL);
	if (!ov02c10)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&ov02c10->sd, client, &ov02c10_subdev_ops);
	ov02c10_get_pm_resources(&client->dev);

	ret = ov02c10_power_on(&client->dev);
	if (ret) {
		dev_err_probe(&client->dev, ret, "failed to power on\n");
		return ret;
	}

	ret = ov02c10_identify_module(ov02c10);
	if (ret) {
		dev_err(&client->dev, "failed to find sensor: %d", ret);
		goto probe_error_ret;
	}

	ov02c10_read_module_name(ov02c10);
	ov02c10_read_mipi_lanes(ov02c10);
	mutex_init(&ov02c10->mutex);
	ov02c10->cur_mode = &supported_modes[0];
	if (ov02c10->mipi_lanes == 2)
		ov02c10->cur_mode = &supported_modes[1];
	ret = ov02c10_init_controls(ov02c10);
	if (ret) {
		dev_err(&client->dev, "failed to init controls: %d", ret);
		goto probe_error_v4l2_ctrl_handler_free;
	}

	ov02c10->sd.internal_ops = &ov02c10_internal_ops;
	ov02c10->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ov02c10->sd.entity.ops = &ov02c10_subdev_entity_ops;
	ov02c10->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ov02c10->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&ov02c10->sd.entity, 1, &ov02c10->pad);
	if (ret) {
		dev_err(&client->dev, "failed to init entity pads: %d", ret);
		goto probe_error_v4l2_ctrl_handler_free;
	}

	ret = v4l2_async_register_subdev_sensor(&ov02c10->sd);
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

	return 0;

probe_error_media_entity_cleanup:
	media_entity_cleanup(&ov02c10->sd.entity);

probe_error_v4l2_ctrl_handler_free:
	v4l2_ctrl_handler_free(ov02c10->sd.ctrl_handler);
	mutex_destroy(&ov02c10->mutex);

probe_error_ret:
	ov02c10_power_off(&client->dev);

	return ret;
}

static const struct dev_pm_ops ov02c10_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ov02c10_suspend, ov02c10_resume)
	SET_RUNTIME_PM_OPS(ov02c10_power_off, ov02c10_power_on, NULL)
};

#ifdef CONFIG_ACPI
static const struct acpi_device_id ov02c10_acpi_ids[] = {
	{"OVTI02C1"},
	{}
};

MODULE_DEVICE_TABLE(acpi, ov02c10_acpi_ids);
#endif

static struct i2c_driver ov02c10_i2c_driver = {
	.driver = {
		.name = "ov02c10",
		.pm = &ov02c10_pm_ops,
		.acpi_match_table = ACPI_PTR(ov02c10_acpi_ids),
	},
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
	.probe_new = ov02c10_probe,
#else
	.probe = ov02c10_probe,
#endif
	.remove = ov02c10_remove,
};

module_i2c_driver(ov02c10_i2c_driver);

MODULE_AUTHOR("Hao Yao <hao.yao@intel.com>");
MODULE_DESCRIPTION("OmniVision OV02C10 sensor driver");
MODULE_LICENSE("GPL v2");
