// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022-2023 Intel Corporation.

#include <asm/unaligned.h>
#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>

#define OV08A10_REG_VALUE_08BIT		1
#define OV08A10_REG_VALUE_16BIT		2
#define OV08A10_REG_VALUE_24BIT		3

#define OV08A10_LINK_FREQ_500MHZ	500000000ULL //1008Mbps
#define OV08A10_SCLK			120000000LL
#define OV08A10_MCLK			19200000
#define OV08A10_DATA_LANES		4
#define OV08A10_RGB_DEPTH		10

#define OV08A10_REG_CHIP_ID		0x300a
#define OV08A10_CHIP_ID			0x560841 //ToDo

#define OV08A10_REG_MODE_SELECT		0x0100
#define OV08A10_MODE_STANDBY		0x00
#define OV08A10_MODE_STREAMING		0x01

/* vertical-timings from sensor */
#define OV08A10_REG_VTS			0x380e
#define OV08A10_VTS_30FPS		0x171a
#define OV08A10_VTS_30FPS_MIN		0x171a
#define OV08A10_VTS_MAX			0x7fff

/* horizontal-timings from sensor */
#define OV08A10_REG_HTS			0x380c

/* Exposure controls from sensor */
#define OV08A10_REG_EXPOSURE		0x3501
#define	OV08A10_EXPOSURE_MIN		8
#define OV08A10_EXPOSURE_MAX_MARGIN	28
#define	OV08A10_EXPOSURE_STEP		1

/* Analog gain controls from sensor */
#define OV08A10_REG_ANALOG_GAIN		0x3508
#define	OV08A10_ANAL_GAIN_MIN		128
#define	OV08A10_ANAL_GAIN_MAX		2047
#define	OV08A10_ANAL_GAIN_STEP		1

/* Digital gain controls from sensor */
#define OV08A10_REG_DIG_GAIN		0x350a
#define OV08A10_DGTL_GAIN_MIN		1024
#define OV08A10_DGTL_GAIN_MAX		16383
#define OV08A10_DGTL_GAIN_STEP		1
#define OV08A10_DGTL_GAIN_DEFAULT	1024

/* Test Pattern Control */
#define OV08A10_REG_TEST_PATTERN	0x5081
#define OV08A10_TEST_PATTERN_ENABLE	BIT(0)
#define OV08A10_TEST_PATTERN_BAR_SHIFT	4

#define to_ov08a10(_sd)			container_of(_sd, struct ov08a10, sd)

enum {
	OV08A10_LINK_FREQ_500MHZ_INDEX,
};

struct ov08a10_reg {
	u16 address;
	u8 val;
};

struct ov08a10_reg_list {
	u32 num_of_regs;
	const struct ov08a10_reg *regs;
};

struct ov08a10_link_freq_config {
	const struct ov08a10_reg_list reg_list;
};

struct ov08a10_mode {
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

	/* Sensor register settings for this resolution */
	const struct ov08a10_reg_list reg_list;
};

static const struct ov08a10_reg ov08a10_global_setting[] = {
	{0x0103, 0x01},
//sl 1 1 ; delay 1ms
	{0x0100, 0x00},
	{0x0102, 0x01},
	{0x0304, 0x01},//01;02;01    ;MCLK=19.2Mhz , 800 Mbps
	{0x0305, 0xe0},//f4;76;f8
	{0x0306, 0x01},
	{0x0307, 0x00},
	{0x0323, 0x04},
	{0x0324, 0x01},
	{0x0325, 0x90},//;40    ;MCLK=19.2Mhz
	{0x4837, 0x15},//14;10;14    ;MCLK=19.2Mhz , 800 Mbps
	{0x3009, 0x06},
	{0x3012, 0x41},
	{0x301e, 0x98},
	{0x3026, 0x10},
	{0x3027, 0x08},
	{0x3106, 0x00},
	{0x3216, 0x01},
	{0x3217, 0x00},
	{0x3218, 0x00},
	{0x3219, 0x55},
	{0x3400, 0x00},
	{0x3408, 0x02},
	{0x340c, 0x20},
	{0x340d, 0x00},
	{0x3410, 0x00},
	{0x3412, 0x00},
	{0x3413, 0x00},
	{0x3414, 0x00},
	{0x3415, 0x00},
	{0x3501, 0x16},
	{0x3502, 0xfa},
	{0x3504, 0x08},
	{0x3508, 0x04},
	{0x3509, 0x00},
	{0x353c, 0x04},
	{0x353d, 0x00},
	{0x3600, 0x20},
	{0x3608, 0x87},
	{0x3609, 0xe0},
	{0x360a, 0x66},
	{0x360c, 0x20},
	{0x361a, 0x80},
	{0x361b, 0xd0},
	{0x361c, 0x11},
	{0x361d, 0x63},
	{0x361e, 0x76},
	{0x3620, 0x50},
	{0x3621, 0x0a},
	{0x3622, 0x8a},
	{0x3625, 0x88},
	{0x3626, 0x49},
	{0x362a, 0x80},
	{0x3632, 0x00},
	{0x3633, 0x10},
	{0x3634, 0x10},
	{0x3635, 0x10},
	{0x3636, 0x0e},
	{0x3659, 0x11},
	{0x365a, 0x23},
	{0x365b, 0x38},
	{0x365c, 0x80},
	{0x3661, 0x0c},
	{0x3663, 0x40},
	{0x3665, 0x12},
	{0x3668, 0xf0},
	{0x3669, 0x0a},
	{0x366a, 0x10},
	{0x366b, 0x43},
	{0x366c, 0x02},
	{0x3674, 0x00},
	{0x3706, 0x1b},
	{0x3709, 0x25},
	{0x370b, 0x3f},
	{0x370c, 0x03},
	{0x3713, 0x02},
	{0x3714, 0x63},
	{0x3726, 0x20},
	{0x373b, 0x06},
	{0x373d, 0x0a},
	{0x3752, 0x00},
	{0x3753, 0x00},
	{0x3754, 0xee},
	{0x3767, 0x08},
	{0x3768, 0x0e},
	{0x3769, 0x02},
	{0x376a, 0x00},
	{0x376b, 0x00},
	{0x37d9, 0x08},
	{0x37dc, 0x00},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x08},
	{0x3804, 0x0c},
	{0x3805, 0xdf},
	{0x3806, 0x09},
	{0x3807, 0xa7},
	{0x3808, 0x0c},
	{0x3809, 0xc0},
	{0x380a, 0x09},
	{0x380b, 0x90},
	{0x380c, 0x03},
	{0x380d, 0x86},
	{0x380e, 0x17},
	{0x380f, 0x1a},
//@@ GRBG
	{0x3810, 0x00},
	{0x3811, 0x11},
	{0x3812, 0x00},
	{0x3813, 0x09},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3820, 0x80},
	{0x3821, 0x04},
	{0x3823, 0x00},
	{0x3824, 0x00},
	{0x3825, 0x00},
	{0x3826, 0x00},
	{0x3827, 0x00},
	{0x382b, 0x08},
	{0x3834, 0xf4},
	{0x3836, 0x14},
	{0x3837, 0x04},
	{0x3898, 0x00},
	{0x38a0, 0x02},
	{0x38a1, 0x02},
	{0x38a2, 0x02},
	{0x38a3, 0x04},
	{0x38c3, 0x00},
	{0x38c4, 0x00},
	{0x38c5, 0x00},
	{0x38c6, 0x00},
	{0x38c7, 0x00},
	{0x38c8, 0x00},
	{0x3d8c, 0x60},
	{0x3d8d, 0x30},
	{0x3f00, 0x8b},
	{0x4000, 0xf7},
	{0x4001, 0x60},
	{0x4002, 0x00},
	{0x4003, 0x40},
	{0x4008, 0x02},
	{0x4009, 0x11},
	{0x400a, 0x01},
	{0x400b, 0x00},
	{0x4020, 0x00},
	{0x4021, 0x00},
	{0x4022, 0x00},
	{0x4023, 0x00},
	{0x4024, 0x00},
	{0x4025, 0x00},
	{0x4026, 0x00},
	{0x4027, 0x00},
	{0x4030, 0x00},
	{0x4031, 0x00},
	{0x4032, 0x00},
	{0x4033, 0x00},
	{0x4034, 0x00},
	{0x4035, 0x00},
	{0x4036, 0x00},
	{0x4037, 0x00},
	{0x4040, 0x00},
	{0x4041, 0x07},
	{0x4201, 0x00},
	{0x4202, 0x00},
	{0x4204, 0x09},
	{0x4205, 0x00},
	{0x4300, 0xff},
	{0x4301, 0x00},
	{0x4302, 0x0f},
	{0x4500, 0x08},
	{0x4501, 0x00},
	{0x450b, 0x00},
	{0x4640, 0x01},
	{0x4641, 0x04},
	{0x4645, 0x03},
	{0x4800, 0x00},
	{0x4803, 0x18},
	{0x4809, 0x2b},
	{0x480e, 0x02},
	{0x4813, 0x90},
	{0x481b, 0x3c},
	{0x4847, 0x01},
	{0x4856, 0x58},
	{0x4888, 0x90},
	{0x4901, 0x00},
	{0x4902, 0x00},
	{0x4904, 0x09},
	{0x4905, 0x00},
	{0x5000, 0x89},
	{0x5001, 0x5a},
	{0x5002, 0x51},
	{0x5005, 0xd0},
	{0x5007, 0xa0},
	{0x500a, 0x02},
	{0x500b, 0x02},
	{0x500c, 0x0a},
	{0x500d, 0x0a},
	{0x500e, 0x02},
	{0x500f, 0x06},
	{0x5010, 0x0a},
	{0x5011, 0x0e},
	{0x5013, 0x00},
	{0x5015, 0x00},
	{0x5017, 0x10},
	{0x5019, 0x00},
	{0x501b, 0xc0},
	{0x501d, 0xa0},
	{0x501e, 0x00},
	{0x501f, 0x40},
	{0x5058, 0x00},
	{0x5081, 0x00},
	{0x5180, 0x00},
	{0x5181, 0x3c},
	{0x5182, 0x01},
	{0x5183, 0xfc},
	{0x5200, 0x4f},
	{0x5203, 0x07},
	{0x5208, 0xff},
	{0x520a, 0x3f},
	{0x520b, 0xc0},
	{0x520c, 0x05},
	{0x520d, 0xc8},
	{0x520e, 0x3f},
	{0x520f, 0x0f},
	{0x5210, 0x0a},
	{0x5218, 0x02},
	{0x5219, 0x01},
	{0x521b, 0x02},
	{0x521c, 0x01},
	{0x58cb, 0x03},
};

static const struct ov08a10_reg mode_3264x2448_regs[] = {
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x08},
	{0x3804, 0x0c},
	{0x3805, 0xdf},
	{0x3806, 0x09},
	{0x3807, 0xa7},
	{0x3808, 0x0c},
	{0x3809, 0xc0},
	{0x380a, 0x09},
	{0x380b, 0x90},
	{0x380c, 0x03},
	{0x380d, 0x86},
	{0x380e, 0x17},
	{0x380f, 0x1a},
//@@ GRBG
	{0x3810, 0x00},
	{0x3811, 0x11},
	{0x3812, 0x00},
	{0x3813, 0x09},
	{0x3814, 0x11},
	{0x3815, 0x11},
};

static const char * const ov08a10_test_pattern_menu[] = {
	"Disabled",
	"Standard Color Bar",
	"Top-Bottom Darker Color Bar",
	"Right-Left Darker Color Bar",
	"Bottom-Top Darker Color Bar"
};

static const s64 link_freq_menu_items[] = {
	OV08A10_LINK_FREQ_500MHZ,
};

static const struct ov08a10_link_freq_config link_freq_configs[] = {
	[OV08A10_LINK_FREQ_500MHZ_INDEX] = {
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(ov08a10_global_setting),
			.regs = ov08a10_global_setting,
		}
	}
};

static const struct ov08a10_mode supported_modes[] = {
	{
		.width = 3264,
		.height = 2448,
		.hts = 902, //0x0386
		.vts_def = OV08A10_VTS_30FPS,
		.vts_min = OV08A10_VTS_30FPS_MIN,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_3264x2448_regs),
			.regs = mode_3264x2448_regs,
		},
		.link_freq_index = OV08A10_LINK_FREQ_500MHZ_INDEX,
	},
};

struct ov08a10 {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;

	/* V4L2 Controls */
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *exposure;

	/* Current mode */
	const struct ov08a10_mode *cur_mode;

	/* To serialize asynchronus callbacks */
	struct mutex mutex;

	/* Streaming on/off */
	bool streaming;

	/* i2c client */
	struct i2c_client *client;

	/* Power management */
	struct clk *imgclk;
	struct gpio_desc *reset_gpio;
	struct regulator *avdd;

	bool identified;
};

static u64 to_pixel_rate(u32 f_index)
{
	u64 pixel_rate = link_freq_menu_items[f_index] * 2 * OV08A10_DATA_LANES;

	do_div(pixel_rate, OV08A10_RGB_DEPTH);

	return pixel_rate;
}

static u64 to_pixels_per_line(u32 hts, u32 f_index)
{
	u64 ppl = hts * to_pixel_rate(f_index);

	do_div(ppl, OV08A10_SCLK);

	return ppl;
}

static int ov08a10_read_reg(struct ov08a10 *ov08a10, u16 reg, u16 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov08a10->sd);
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

static int ov08a10_write_reg(struct ov08a10 *ov08a10, u16 reg, u16 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov08a10->sd);
	u8 buf[6];

	if (len > 4)
		return -EINVAL;

	put_unaligned_be16(reg, buf);
	put_unaligned_be32(val << 8 * (4 - len), buf + 2);
	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

static int ov08a10_write_reg_list(struct ov08a10 *ov08a10,
				  const struct ov08a10_reg_list *r_list)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov08a10->sd);
	unsigned int i;
	int ret;

	for (i = 0; i < r_list->num_of_regs; i++) {
		ret = ov08a10_write_reg(ov08a10, r_list->regs[i].address, 1,
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

static int ov08a10_test_pattern(struct ov08a10 *ov08a10, u32 pattern)
{
	if (pattern)
		pattern = (pattern - 1) << OV08A10_TEST_PATTERN_BAR_SHIFT |
			  OV08A10_TEST_PATTERN_ENABLE;

	return ov08a10_write_reg(ov08a10, OV08A10_REG_TEST_PATTERN,
				 OV08A10_REG_VALUE_08BIT, pattern);
}

static int ov08a10_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov08a10 *ov08a10 = container_of(ctrl->handler,
					       struct ov08a10, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&ov08a10->sd);
	s64 exposure_max;
	int ret = 0;

	/* Propagate change of current control to all related controls */
	if (ctrl->id == V4L2_CID_VBLANK) {
		/* Update max exposure while meeting expected vblanking */
		exposure_max = ov08a10->cur_mode->height + ctrl->val -
			       OV08A10_EXPOSURE_MAX_MARGIN;
		__v4l2_ctrl_modify_range(ov08a10->exposure,
					 ov08a10->exposure->minimum,
					 exposure_max, ov08a10->exposure->step,
					 exposure_max);
	}

	/* V4L2 controls values will be applied only when power is already up */
	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		ret = ov08a10_write_reg(ov08a10, OV08A10_REG_ANALOG_GAIN,
					OV08A10_REG_VALUE_16BIT,
					ctrl->val << 1);
		break;

	case V4L2_CID_DIGITAL_GAIN:
		ret = ov08a10_write_reg(ov08a10, OV08A10_REG_DIG_GAIN,
					OV08A10_REG_VALUE_24BIT,
					ctrl->val << 6);
		break;

	case V4L2_CID_EXPOSURE:
		ret = ov08a10_write_reg(ov08a10, OV08A10_REG_EXPOSURE,
					OV08A10_REG_VALUE_16BIT, ctrl->val);
		break;

	case V4L2_CID_VBLANK:
		ret = ov08a10_write_reg(ov08a10, OV08A10_REG_VTS,
					OV08A10_REG_VALUE_16BIT,
					ov08a10->cur_mode->height + ctrl->val);
		break;

	case V4L2_CID_TEST_PATTERN:
		ret = ov08a10_test_pattern(ov08a10, ctrl->val);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops ov08a10_ctrl_ops = {
	.s_ctrl = ov08a10_set_ctrl,
};

static int ov08a10_init_controls(struct ov08a10 *ov08a10)
{
	struct v4l2_ctrl_handler *ctrl_hdlr;
	const struct ov08a10_mode *cur_mode;
	s64 exposure_max, h_blank;
	int ret = 0;
	int size;

	ctrl_hdlr = &ov08a10->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 8);
	if (ret)
		return ret;

	ctrl_hdlr->lock = &ov08a10->mutex;
	cur_mode = ov08a10->cur_mode;
	size = ARRAY_SIZE(link_freq_menu_items);
	ov08a10->link_freq = v4l2_ctrl_new_int_menu(ctrl_hdlr,
						    &ov08a10_ctrl_ops,
						    V4L2_CID_LINK_FREQ,
						    ARRAY_SIZE
						    (link_freq_menu_items) - 1,
						    0, link_freq_menu_items);
	if (ov08a10->link_freq)
		ov08a10->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	ov08a10->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &ov08a10_ctrl_ops,
						V4L2_CID_PIXEL_RATE, 0,
						to_pixel_rate
						(OV08A10_LINK_FREQ_500MHZ_INDEX),
						1,
						to_pixel_rate
						(OV08A10_LINK_FREQ_500MHZ_INDEX));

	ov08a10->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &ov08a10_ctrl_ops,
					    V4L2_CID_VBLANK,
					    ov08a10->cur_mode->vts_min,
					    OV08A10_VTS_MAX, 1,
					    ov08a10->cur_mode->vts_def);

	h_blank = to_pixels_per_line(ov08a10->cur_mode->hts,
				     ov08a10->cur_mode->link_freq_index) -
				     ov08a10->cur_mode->width;
	ov08a10->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &ov08a10_ctrl_ops,
					    V4L2_CID_HBLANK, h_blank, h_blank,
					    1, h_blank);
	if (ov08a10->hblank)
		ov08a10->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	v4l2_ctrl_new_std(ctrl_hdlr, &ov08a10_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  OV08A10_ANAL_GAIN_MIN, OV08A10_ANAL_GAIN_MAX,
			  OV08A10_ANAL_GAIN_STEP, OV08A10_ANAL_GAIN_MIN);

	v4l2_ctrl_new_std(ctrl_hdlr, &ov08a10_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  OV08A10_DGTL_GAIN_MIN, OV08A10_DGTL_GAIN_MAX,
			  OV08A10_DGTL_GAIN_STEP, OV08A10_DGTL_GAIN_DEFAULT);
	exposure_max = (ov08a10->cur_mode->vts_def -
			OV08A10_EXPOSURE_MAX_MARGIN);

	ov08a10->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &ov08a10_ctrl_ops,
					      V4L2_CID_EXPOSURE,
					      OV08A10_EXPOSURE_MIN,
					      exposure_max,
					      OV08A10_EXPOSURE_STEP,
					      exposure_max);
	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &ov08a10_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(ov08a10_test_pattern_menu) - 1,
				     0, 0, ov08a10_test_pattern_menu);

	if (ctrl_hdlr->error)
		return ctrl_hdlr->error;

	ov08a10->sd.ctrl_handler = ctrl_hdlr;

	return 0;
}

static void ov08a10_update_pad_format(const struct ov08a10_mode *mode,
				      struct v4l2_mbus_framefmt *fmt)
{
	fmt->width = mode->width;
	fmt->height = mode->height;
	fmt->code = MEDIA_BUS_FMT_SGRBG10_1X10;
	fmt->field = V4L2_FIELD_NONE;
}

static int ov08a10_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ov08a10 *ov08a10 = to_ov08a10(sd);

	gpiod_set_value_cansleep(ov08a10->reset_gpio, 1);
	if (ov08a10->avdd)
		regulator_disable(ov08a10->avdd);

	clk_disable_unprepare(ov08a10->imgclk);
	msleep(50);

	return 0;
}

static int ov08a10_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ov08a10 *ov08a10 = to_ov08a10(sd);
	int ret;

	ret = clk_prepare_enable(ov08a10->imgclk);
	if (ret < 0) {
		dev_err(dev, "failed to enable imgclk: %d", ret);
		return ret;
	}

	if (ov08a10->avdd)
		ret = regulator_enable(ov08a10->avdd);
	if (ret < 0) {
		dev_err(dev, "failed to enable avdd: %d", ret);
		return ret;
	}

	gpiod_set_value_cansleep(ov08a10->reset_gpio, 0);
	msleep(50);

	return 0;
}

static int ov08a10_start_streaming(struct ov08a10 *ov08a10)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov08a10->sd);
	const struct ov08a10_reg_list *reg_list;
	int link_freq_index, ret;

	link_freq_index = ov08a10->cur_mode->link_freq_index;
	reg_list = &link_freq_configs[link_freq_index].reg_list;

	ret = ov08a10_write_reg_list(ov08a10, reg_list);
	if (ret) {
		dev_err(&client->dev, "failed to set plls");
		return ret;
	}

	reg_list = &ov08a10->cur_mode->reg_list;
	ret = ov08a10_write_reg_list(ov08a10, reg_list);
	if (ret) {
		dev_err(&client->dev, "failed to set mode");
		return ret;
	}

	ret = __v4l2_ctrl_handler_setup(ov08a10->sd.ctrl_handler);
	if (ret)
		return ret;

	ret = ov08a10_write_reg(ov08a10, OV08A10_REG_MODE_SELECT,
				OV08A10_REG_VALUE_08BIT,
				OV08A10_MODE_STREAMING);
	if (ret) {
		dev_err(&client->dev, "failed to set stream");
		return ret;
	}

	return 0;
}

static void ov08a10_stop_streaming(struct ov08a10 *ov08a10)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov08a10->sd);

	if (ov08a10_write_reg(ov08a10, OV08A10_REG_MODE_SELECT,
			      OV08A10_REG_VALUE_08BIT, OV08A10_MODE_STANDBY))
		dev_err(&client->dev, "failed to set stream");
}

static int ov08a10_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov08a10 *ov08a10 = to_ov08a10(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (ov08a10->streaming == enable)
		return 0;

	mutex_lock(&ov08a10->mutex);
	if (enable) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			mutex_unlock(&ov08a10->mutex);
			return ret;
		}

		ret = ov08a10_start_streaming(ov08a10);
		if (ret) {
			dev_err(&client->dev, "start streaming failed\n");
			enable = 0;
			ov08a10_stop_streaming(ov08a10);
			pm_runtime_put(&client->dev);
		}
	} else {
		ov08a10_stop_streaming(ov08a10);
		pm_runtime_put(&client->dev);
	}

	ov08a10->streaming = enable;
	mutex_unlock(&ov08a10->mutex);

	return ret;
}

static int __maybe_unused ov08a10_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov08a10 *ov08a10 = to_ov08a10(sd);

	mutex_lock(&ov08a10->mutex);
	if (ov08a10->streaming)
		ov08a10_stop_streaming(ov08a10);

	mutex_unlock(&ov08a10->mutex);

	return 0;
}

static int __maybe_unused ov08a10_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov08a10 *ov08a10 = to_ov08a10(sd);
	int ret;

	mutex_lock(&ov08a10->mutex);
	if (ov08a10->streaming) {
		ret = ov08a10_start_streaming(ov08a10);
		if (ret) {
			ov08a10->streaming = false;
			ov08a10_stop_streaming(ov08a10);
			mutex_unlock(&ov08a10->mutex);
			return ret;
		}
	}

	mutex_unlock(&ov08a10->mutex);

	return 0;
}

static int ov08a10_set_format(struct v4l2_subdev *sd,
			      struct v4l2_subdev_state *sd_state,
			      struct v4l2_subdev_format *fmt)
{
	struct ov08a10 *ov08a10 = to_ov08a10(sd);
	const struct ov08a10_mode *mode;
	s32 vblank_def, h_blank;

	mode = v4l2_find_nearest_size(supported_modes,
				      ARRAY_SIZE(supported_modes), width,
				      height, fmt->format.width,
				      fmt->format.height);

	mutex_lock(&ov08a10->mutex);
	ov08a10_update_pad_format(mode, &fmt->format);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
		*v4l2_subdev_get_try_format(sd, sd_state,
					    fmt->pad) = fmt->format;
#else
		*v4l2_subdev_state_get_format(sd_state,
					    fmt->pad) = fmt->format;
#endif
	} else {
		ov08a10->cur_mode = mode;
		__v4l2_ctrl_s_ctrl(ov08a10->link_freq, mode->link_freq_index);
		__v4l2_ctrl_s_ctrl_int64(ov08a10->pixel_rate,
					 to_pixel_rate(mode->link_freq_index));

		/* Update limits and set FPS to default */
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(ov08a10->vblank,
					 mode->vts_min - mode->height,
					 OV08A10_VTS_MAX - mode->height, 1,
					 vblank_def);
		__v4l2_ctrl_s_ctrl(ov08a10->vblank, vblank_def);
		h_blank = to_pixels_per_line(mode->hts, mode->link_freq_index) -
			  mode->width;
		__v4l2_ctrl_modify_range(ov08a10->hblank, h_blank, h_blank, 1,
					 h_blank);
	}

	mutex_unlock(&ov08a10->mutex);

	return 0;
}

static int ov08a10_get_format(struct v4l2_subdev *sd,
			      struct v4l2_subdev_state *sd_state,
			      struct v4l2_subdev_format *fmt)
{
	struct ov08a10 *ov08a10 = to_ov08a10(sd);

	mutex_lock(&ov08a10->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
		fmt->format = *v4l2_subdev_get_try_format(&ov08a10->sd,
							  sd_state,
							  fmt->pad);
#else
		fmt->format = *v4l2_subdev_state_get_format(
							  sd_state,
							  fmt->pad);
#endif
	else
		ov08a10_update_pad_format(ov08a10->cur_mode, &fmt->format);

	mutex_unlock(&ov08a10->mutex);

	return 0;
}

static int ov08a10_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index > 0)
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_SGRBG10_1X10;

	return 0;
}

static int ov08a10_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
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

static int ov08a10_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ov08a10 *ov08a10 = to_ov08a10(sd);

	mutex_lock(&ov08a10->mutex);
	ov08a10_update_pad_format(&supported_modes[0],
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
				  v4l2_subdev_get_try_format(sd, fh->state, 0));
#else
				  v4l2_subdev_state_get_format(fh->state, 0));
#endif
	mutex_unlock(&ov08a10->mutex);

	return 0;
}

static const struct v4l2_subdev_video_ops ov08a10_video_ops = {
	.s_stream = ov08a10_set_stream,
};

static const struct v4l2_subdev_pad_ops ov08a10_pad_ops = {
	.set_fmt = ov08a10_set_format,
	.get_fmt = ov08a10_get_format,
	.enum_mbus_code = ov08a10_enum_mbus_code,
	.enum_frame_size = ov08a10_enum_frame_size,
};

static const struct v4l2_subdev_ops ov08a10_subdev_ops = {
	.video = &ov08a10_video_ops,
	.pad = &ov08a10_pad_ops,
};

static const struct media_entity_operations ov08a10_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_subdev_internal_ops ov08a10_internal_ops = {
	.open = ov08a10_open,
};

static int ov08a10_identify_module(struct ov08a10 *ov08a10)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov08a10->sd);
	int ret;
	u32 val;

	if (ov08a10->identified)
		return 0;

	ret = ov08a10_read_reg(ov08a10, OV08A10_REG_CHIP_ID,
			       OV08A10_REG_VALUE_24BIT, &val);
	if (ret)
		return ret;

	if (val != OV08A10_CHIP_ID) {
		dev_err(&client->dev, "chip id mismatch: %x!=%x",
			OV08A10_CHIP_ID, val);
		return -ENXIO;
	}

	ov08a10->identified = true;

	return 0;
}

static int ov08a10_check_hwcfg(struct device *dev)
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

	if (bus_cfg.bus.mipi_csi2.num_data_lanes != OV08A10_DATA_LANES) {
		dev_err(dev, "number of CSI2 data lanes %d is not supported",
			bus_cfg.bus.mipi_csi2.num_data_lanes);
		ret = -EINVAL;
		goto out_err;
	}

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
static int ov08a10_remove(struct i2c_client *client)
#else
static void ov08a10_remove(struct i2c_client *client)
#endif

{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov08a10 *ov08a10 = to_ov08a10(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);
	pm_runtime_disable(&client->dev);
	mutex_destroy(&ov08a10->mutex);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

static void ov08a10_get_hwcfg(struct ov08a10 *ov08a10, struct device *dev)
{
	ov08a10->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						      GPIOD_OUT_LOW);
	if (IS_ERR(ov08a10->reset_gpio)) {
		dev_dbg(dev, "could not get gpio reset: %ld",
			PTR_ERR(ov08a10->reset_gpio));
		ov08a10->reset_gpio = NULL;
	}

	ov08a10->imgclk = devm_clk_get_optional(dev, NULL);
	if (IS_ERR(ov08a10->imgclk)) {
		dev_dbg(dev, "could not get imgclk: %ld",
			PTR_ERR(ov08a10->imgclk));
		ov08a10->imgclk = NULL;
	}

	ov08a10->avdd = devm_regulator_get_optional(dev, "avdd");
	if (IS_ERR(ov08a10->avdd)) {
		dev_dbg(dev, "could not get regulator avdd: %ld",
			PTR_ERR(ov08a10->avdd));
		ov08a10->avdd = NULL;
	}
}

static int ov08a10_probe(struct i2c_client *client)
{
	struct ov08a10 *ov08a10;
	int ret = 0;

	/* Check HW config */
	ret = ov08a10_check_hwcfg(&client->dev);
	if (ret) {
		dev_err(&client->dev, "failed to check hwcfg: %d", ret);
		return ret;
	}

	ov08a10 = devm_kzalloc(&client->dev, sizeof(*ov08a10), GFP_KERNEL);
	if (!ov08a10)
		return -ENOMEM;
	ov08a10->client = client;

	ov08a10_get_hwcfg(ov08a10, &client->dev);
	v4l2_i2c_subdev_init(&ov08a10->sd, client, &ov08a10_subdev_ops);
	ov08a10_power_on(&client->dev);
	ret = ov08a10_identify_module(ov08a10);
	if (ret) {
		dev_err(&client->dev, "failed to find sensor: %d", ret);
		goto probe_error_power_down;
	}

	mutex_init(&ov08a10->mutex);

	ov08a10->cur_mode = &supported_modes[0];
	ret = ov08a10_init_controls(ov08a10);
	if (ret) {
		dev_err(&client->dev, "failed to init controls: %d", ret);
		goto probe_error_v4l2_ctrl_handler_free;
	}

	ov08a10->sd.internal_ops = &ov08a10_internal_ops;
	ov08a10->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ov08a10->sd.entity.ops = &ov08a10_subdev_entity_ops;
	ov08a10->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ov08a10->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&ov08a10->sd.entity, 1, &ov08a10->pad);
	if (ret) {
		dev_err(&client->dev, "failed to init entity pads: %d", ret);
		goto probe_error_v4l2_ctrl_handler_free;
	}

	ret = v4l2_async_register_subdev_sensor(&ov08a10->sd);
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
	dev_err(&client->dev, "media_entity_cleanup , probe error\n");
	media_entity_cleanup(&ov08a10->sd.entity);

probe_error_v4l2_ctrl_handler_free:
	dev_err(&client->dev, "v4l2_ctrl_handle_free\n");
	v4l2_ctrl_handler_free(ov08a10->sd.ctrl_handler);
	mutex_destroy(&ov08a10->mutex);

probe_error_power_down:
	ov08a10_power_off(&client->dev);

	return ret;
}

static const struct dev_pm_ops ov08a10_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ov08a10_suspend, ov08a10_resume)
	SET_RUNTIME_PM_OPS(ov08a10_power_off, ov08a10_power_on, NULL)
};

#ifdef CONFIG_ACPI
static const struct acpi_device_id ov08a10_acpi_ids[] = {
	{"OVTI08A1"},
	{}
};

MODULE_DEVICE_TABLE(acpi, ov08a10_acpi_ids);
#endif

static struct i2c_driver ov08a10_i2c_driver = {
	.driver = {
		.name = "ov08a10",
		.pm = &ov08a10_pm_ops,
		.acpi_match_table = ov08a10_acpi_ids,
	},
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
	.probe_new = ov08a10_probe,
#else
	.probe = ov08a10_probe,
#endif
	.remove = ov08a10_remove,
};

module_i2c_driver(ov08a10_i2c_driver);

MODULE_AUTHOR("Jason Chen <jason.z.chen@intel.com>");
MODULE_AUTHOR("Shawn Tu <shawnx.tu@intel.com>");
MODULE_DESCRIPTION("OmniVision OV08A10 sensor driver");
MODULE_LICENSE("GPL v2");
