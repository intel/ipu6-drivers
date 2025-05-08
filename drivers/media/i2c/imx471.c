// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2025 Intel Corporation

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
#include <asm/unaligned.h>
#else
#include <linux/unaligned.h>
#endif
#include <linux/acpi.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>

#define IMX471_REG_MODE_SELECT		0x0100
#define IMX471_MODE_STANDBY		0x00
#define IMX471_MODE_STREAMING		0x01

/* Chip ID */
#define IMX471_REG_CHIP_ID		0x0016
#define IMX471_CHIP_ID			0x0471

/* V_TIMING internal */
#define IMX471_REG_FLL			0x0340
#define IMX471_FLL_MAX			0xffff

/* Exposure control */
#define IMX471_REG_EXPOSURE		0x0202
#define IMX471_EXPOSURE_MIN		1
#define IMX471_EXPOSURE_STEP		1
#define IMX471_EXPOSURE_DEFAULT		0x04f6

/*
 *  the digital control register for all color control looks like:
 *  +-----------------+------------------+
 *  |      [7:0]      |       [15:8]     |
 *  +-----------------+------------------+
 *  |	  0x020f      |       0x020e     |
 *  --------------------------------------
 *  it is used to calculate the digital gain times value(integral + fractional)
 *  the [15:8] bits is the fractional part and [7:0] bits is the integral
 *  calculation equation is:
 *      gain value (unit: times) = REG[15:8] + REG[7:0]/0x100
 *  Only value in 0x0100 ~ 0x0FFF range is allowed.
 *  Analog gain use 10 bits in the registers and allowed range is 0 ~ 960
 */
/* Analog gain control */
#define IMX471_REG_ANALOG_GAIN		0x0204
#define IMX471_ANA_GAIN_MIN		0
#define IMX471_ANA_GAIN_MAX		960
#define IMX471_ANA_GAIN_STEP		1
#define IMX471_ANA_GAIN_DEFAULT		0

/* Digital gain control */
#define IMX471_REG_DPGA_USE_GLOBAL_GAIN	0x3ff9
#define IMX471_REG_DIG_GAIN_GLOBAL	0x020e
#define IMX471_DGTL_GAIN_MIN		256
#define IMX471_DGTL_GAIN_MAX		4095
#define IMX471_DGTL_GAIN_STEP		1
#define IMX471_DGTL_GAIN_DEFAULT	256

/* Test Pattern Control */
#define IMX471_REG_TEST_PATTERN		0x0600
#define IMX471_TEST_PATTERN_DISABLED		0
#define IMX471_TEST_PATTERN_SOLID_COLOR		1
#define IMX471_TEST_PATTERN_COLOR_BARS		2
#define IMX471_TEST_PATTERN_GRAY_COLOR_BARS	3
#define IMX471_TEST_PATTERN_PN9			4

/* default link frequency and external clock */
#define IMX471_LINK_FREQ_DEFAULT	200000000LL
#define IMX471_EXT_CLK			19200000
#define IMX471_LINK_FREQ_INDEX		0

struct imx471_reg {
	u16 address;
	u8 val;
};

struct imx471_reg_list {
	u32 num_of_regs;
	const struct imx471_reg *regs;
};

/* Mode : resolution and related config&values */
struct imx471_mode {
	/* Frame width */
	u32 width;
	/* Frame height */
	u32 height;

	/* V-timing */
	u32 fll_def;
	u32 fll_min;

	/* H-timing */
	u32 llp;

	/* index of link frequency */
	u32 link_freq_index;

	/* Default register values */
	struct imx471_reg_list reg_list;
};

struct imx471_hwcfg {
	u32 ext_clk;			/* sensor external clk */
	unsigned long link_freq_bitmap;
};

struct imx471 {
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
	const struct imx471_mode *cur_mode;

	struct imx471_hwcfg *hwcfg;

	/*
	 * Mutex for serialized access:
	 * Protect sensor set pad format and start/stop streaming safely.
	 * Protect access to sensor v4l2 controls.
	 */
	struct mutex mutex;

	/* True if the device has been identified */
	bool identified;
};

static const struct imx471_reg imx471_global_regs[] = {
	{0x0136, 0x13},
	{0x0137, 0x33},
	{0x3c7e, 0x08},
	{0x3c7f, 0x05},
	{0x3e35, 0x00},
	{0x3e36, 0x00},
	{0x3e37, 0x00},
	{0x3f7f, 0x01},
	{0x4431, 0x04},
	{0x531c, 0x01},
	{0x531d, 0x02},
	{0x531e, 0x04},
	{0x5928, 0x00},
	{0x5929, 0x2f},
	{0x592a, 0x00},
	{0x592b, 0x85},
	{0x592c, 0x00},
	{0x592d, 0x32},
	{0x592e, 0x00},
	{0x592f, 0x88},
	{0x5930, 0x00},
	{0x5931, 0x3d},
	{0x5932, 0x00},
	{0x5933, 0x93},
	{0x5938, 0x00},
	{0x5939, 0x24},
	{0x593a, 0x00},
	{0x593b, 0x7a},
	{0x593c, 0x00},
	{0x593d, 0x24},
	{0x593e, 0x00},
	{0x593f, 0x7a},
	{0x5940, 0x00},
	{0x5941, 0x2f},
	{0x5942, 0x00},
	{0x5943, 0x85},
	{0x5f0e, 0x6e},
	{0x5f11, 0xc6},
	{0x5f17, 0x5e},
	{0x7990, 0x01},
	{0x7993, 0x5d},
	{0x7994, 0x5d},
	{0x7995, 0xa1},
	{0x799a, 0x01},
	{0x799d, 0x00},
	{0x8169, 0x01},
	{0x8359, 0x01},
	{0x9302, 0x1e},
	{0x9306, 0x1f},
	{0x930a, 0x26},
	{0x930e, 0x23},
	{0x9312, 0x23},
	{0x9316, 0x2c},
	{0x9317, 0x19},
	{0xb046, 0x01},
	{0xb048, 0x01},
};

static const struct imx471_reg_list imx471_global_setting = {
	.num_of_regs = ARRAY_SIZE(imx471_global_regs),
	.regs = imx471_global_regs,
};

static const struct imx471_reg mode_1928x1088_regs[] = {
	{0x0101, 0x02},
	{0x0112, 0x0a},
	{0x0113, 0x0a},
	{0x0114, 0x03},
	{0x0342, 0x0a},
	{0x0343, 0xe0},
	{0x0340, 0x04},
	{0x0341, 0xec},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x01},
	{0x0347, 0xba},
	{0x0348, 0x12},
	{0x0349, 0x2f},
	{0x034a, 0x0b},
	{0x034b, 0xe0},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x01},
	{0x0901, 0x22},
	{0x0902, 0x08},
	{0x3f4c, 0x81},
	{0x3f4d, 0x81},
	{0x0408, 0x00},
	{0x0409, 0xc9},
	{0x040a, 0x00},
	{0x040b, 0x6d},
	{0x040c, 0x07},
	{0x040d, 0x88},
	{0x040e, 0x04},
	{0x040f, 0x40},
	{0x034c, 0x07},
	{0x034d, 0x88},
	{0x034e, 0x04},
	{0x034f, 0x40},
	{0x0301, 0x06},
	{0x0303, 0x02},
	{0x0305, 0x02},
	{0x0306, 0x00},
	{0x0307, 0x3c},
	{0x030b, 0x01},
	{0x030d, 0x02},
	{0x030e, 0x00},
	{0x030f, 0x53},
	{0x0310, 0x01},
	{0x0202, 0x13},
	{0x0203, 0x9e},
	{0x0204, 0x00},
	{0x0205, 0x00},
	{0x020e, 0x01},
	{0x020f, 0x00},
	{0x3f78, 0x01},
	{0x3f79, 0x31},
	{0x3ffe, 0x00},
	{0x3fff, 0x8a},
	{0x5f0a, 0xb6},
};

static const char * const imx471_test_pattern_menu[] = {
	"Disabled",
	"Solid Colour",
	"Eight Vertical Colour Bars",
	"Colour Bars With Fade to Grey",
	"Pseudorandom Sequence (PN9)",
};

/*
 * When adding more than the one below, make sure the disallowed ones will
 * actually be disabled in the LINK_FREQ control.
 */
static const s64 link_freq_menu_items[] = {
	IMX471_LINK_FREQ_DEFAULT,
};

/* Mode configs */
static const struct imx471_mode supported_modes[] = {
	{
		.width = 1928,
		.height = 1088,
		.fll_def = 1308,
		.fll_min = 1308,
		.llp = 2328,
		.link_freq_index = IMX471_LINK_FREQ_INDEX,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1928x1088_regs),
			.regs = mode_1928x1088_regs,
		},
	},
};

static inline struct imx471 *to_imx471(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct imx471, sd);
}

/* Read registers up to 4 at a time */
static int imx471_read_reg(struct imx471 *imx471, u16 reg, u32 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx471->sd);
	struct i2c_msg msgs[2];
	u8 addr_buf[2];
	u8 data_buf[4] = { 0 };
	int ret;

	if (len > 4)
		return -EINVAL;

	put_unaligned_be16(reg, addr_buf);
	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = ARRAY_SIZE(addr_buf);
	msgs[0].buf = addr_buf;

	/* Read data from register */
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

/* Write registers up to 4 at a time */
static int imx471_write_reg(struct imx471 *imx471, u16 reg, u32 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx471->sd);
	u8 buf[6];

	if (len > 4)
		return -EINVAL;

	put_unaligned_be16(reg, buf);
	put_unaligned_be32(val << (8 * (4 - len)), buf + 2);
	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

/* Write a list of registers */
static int imx471_write_regs(struct imx471 *imx471,
			     const struct imx471_reg *regs, u32 len)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx471->sd);
	int ret;
	u32 i;

	for (i = 0; i < len; i++) {
		ret = imx471_write_reg(imx471, regs[i].address, 1, regs[i].val);
		if (ret) {
			dev_err_ratelimited(&client->dev,
					    "write reg 0x%4.4x return err %d",
					    regs[i].address, ret);
			return ret;
		}
	}

	return 0;
}

/* Open sub-device */
static int imx471_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct imx471 *imx471 = to_imx471(sd);
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_state_get_format(fh->state, 0);

	mutex_lock(&imx471->mutex);

	/* Initialize try_fmt */
	try_fmt->width = imx471->cur_mode->width;
	try_fmt->height = imx471->cur_mode->height;
	try_fmt->code = MEDIA_BUS_FMT_SRGGB10_1X10;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&imx471->mutex);

	return 0;
}

static int imx471_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx471 *imx471 = container_of(ctrl->handler,
					     struct imx471, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&imx471->sd);
	s64 max;
	int ret;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = imx471->cur_mode->height + ctrl->val - 18;
		__v4l2_ctrl_modify_range(imx471->exposure,
					 imx471->exposure->minimum,
					 max, imx471->exposure->step, max);
		break;
	}

	/*
	 * Applying V4L2 control value only happens
	 * when power is up for streaming
	 */
	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		/* Analog gain = 1024/(1024 - ctrl->val) times */
		ret = imx471_write_reg(imx471, IMX471_REG_ANALOG_GAIN, 2,
				       ctrl->val);
		break;
	case V4L2_CID_DIGITAL_GAIN:
		ret = imx471_write_reg(imx471, IMX471_REG_DIG_GAIN_GLOBAL, 2,
				       ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		ret = imx471_write_reg(imx471, IMX471_REG_EXPOSURE, 2,
				       ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		/* Update FLL that meets expected vertical blanking */
		ret = imx471_write_reg(imx471, IMX471_REG_FLL, 2,
				       imx471->cur_mode->height + ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = imx471_write_reg(imx471, IMX471_REG_TEST_PATTERN,
				       2, ctrl->val);
		break;
	default:
		ret = -EINVAL;
		dev_info(&client->dev, "ctrl(id:0x%x,val:0x%x) is not handled",
			 ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx471_ctrl_ops = {
	.s_ctrl = imx471_set_ctrl,
};

static int imx471_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx471 *imx471 = to_imx471(sd);

	if (code->index > 0)
		return -EINVAL;

	mutex_lock(&imx471->mutex);
	code->code = MEDIA_BUS_FMT_SRGGB10_1X10;
	mutex_unlock(&imx471->mutex);

	return 0;
}

static int imx471_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = supported_modes[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

static void imx471_update_pad_format(struct imx471 *imx471,
				     const struct imx471_mode *mode,
				     struct v4l2_subdev_format *fmt)
{
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
	fmt->format.field = V4L2_FIELD_NONE;
}

static int imx471_do_get_pad_format(struct imx471 *imx471,
				    struct v4l2_subdev_state *sd_state,
				    struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		framefmt = v4l2_subdev_state_get_format(sd_state, fmt->pad);
		fmt->format = *framefmt;
	} else {
		imx471_update_pad_format(imx471, imx471->cur_mode, fmt);
	}

	return 0;
}

static int imx471_get_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_format *fmt)
{
	struct imx471 *imx471 = to_imx471(sd);
	int ret;

	mutex_lock(&imx471->mutex);
	ret = imx471_do_get_pad_format(imx471, sd_state, fmt);
	mutex_unlock(&imx471->mutex);

	return ret;
}

static int
imx471_set_pad_format(struct v4l2_subdev *sd,
		      struct v4l2_subdev_state *sd_state,
		      struct v4l2_subdev_format *fmt)
{
	struct imx471 *imx471 = to_imx471(sd);
	const struct imx471_mode *mode;
	struct v4l2_mbus_framefmt *framefmt;
	s32 vblank_def;
	s32 vblank_min;
	s64 h_blank;
	u64 pixel_rate;
	u32 height;

	mutex_lock(&imx471->mutex);

	fmt->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;

	mode = v4l2_find_nearest_size(supported_modes,
				      ARRAY_SIZE(supported_modes),
				      width, height,
				      fmt->format.width, fmt->format.height);
	imx471_update_pad_format(imx471, mode, fmt);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		framefmt = v4l2_subdev_state_get_format(sd_state, fmt->pad);
		*framefmt = fmt->format;
	} else {
		imx471->cur_mode = mode;
		pixel_rate = IMX471_LINK_FREQ_DEFAULT * 2 * 4;
		do_div(pixel_rate, 10);
		__v4l2_ctrl_s_ctrl_int64(imx471->pixel_rate, pixel_rate);
		/* Update limits and set FPS to default */
		height = imx471->cur_mode->height;
		vblank_def = imx471->cur_mode->fll_def - height;
		vblank_min = imx471->cur_mode->fll_min - height;
		height = IMX471_FLL_MAX - height;
		__v4l2_ctrl_modify_range(imx471->vblank, vblank_min, height, 1,
					 vblank_def);
		__v4l2_ctrl_s_ctrl(imx471->vblank, vblank_def);
		h_blank = mode->llp - imx471->cur_mode->width;
		/*
		 * Currently hblank is not changeable.
		 * So FPS control is done only by vblank.
		 */
		__v4l2_ctrl_modify_range(imx471->hblank, h_blank,
					 h_blank, 1, h_blank);
	}

	mutex_unlock(&imx471->mutex);

	return 0;
}

/* Verify chip ID */
static int imx471_identify_module(struct imx471 *imx471)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx471->sd);
	int ret;
	u32 val;

	if (imx471->identified)
		return 0;

	ret = imx471_read_reg(imx471, IMX471_REG_CHIP_ID, 2, &val);
	if (ret)
		return ret;

	if (val != IMX471_CHIP_ID) {
		dev_err(&client->dev, "chip id mismatch: %x!=%x",
			IMX471_CHIP_ID, val);
		return -EIO;
	}

	imx471->identified = true;

	return 0;
}

/* Start streaming */
static int imx471_start_streaming(struct imx471 *imx471)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx471->sd);
	const struct imx471_reg_list *reg_list;
	int ret;

	ret = imx471_identify_module(imx471);
	if (ret)
		return ret;

	/* Global Setting */
	reg_list = &imx471_global_setting;
	ret = imx471_write_regs(imx471, reg_list->regs, reg_list->num_of_regs);
	if (ret) {
		dev_err(&client->dev, "failed to set global settings");
		return ret;
	}

	/* Apply default values of current mode */
	reg_list = &imx471->cur_mode->reg_list;
	ret = imx471_write_regs(imx471, reg_list->regs, reg_list->num_of_regs);
	if (ret) {
		dev_err(&client->dev, "failed to set mode");
		return ret;
	}

	/* set digital gain control to all color mode */
	ret = imx471_write_reg(imx471, IMX471_REG_DPGA_USE_GLOBAL_GAIN, 1, 1);
	if (ret)
		return ret;

	/* Apply customized values from user */
	ret =  __v4l2_ctrl_handler_setup(imx471->sd.ctrl_handler);
	if (ret)
		return ret;

	return imx471_write_reg(imx471, IMX471_REG_MODE_SELECT,
				1, IMX471_MODE_STREAMING);
}

/* Stop streaming */
static int imx471_stop_streaming(struct imx471 *imx471)
{
	return imx471_write_reg(imx471, IMX471_REG_MODE_SELECT,
				1, IMX471_MODE_STANDBY);
}

static int imx471_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx471 *imx471 = to_imx471(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	mutex_lock(&imx471->mutex);

	if (enable) {
		ret = pm_runtime_resume_and_get(&client->dev);
		if (ret < 0)
			goto err_unlock;

		/*
		 * Apply default & customized values
		 * and then start streaming.
		 */
		ret = imx471_start_streaming(imx471);
		if (ret)
			goto err_rpm_put;
	} else {
		imx471_stop_streaming(imx471);
		pm_runtime_put(&client->dev);
	}

	mutex_unlock(&imx471->mutex);

	return ret;

err_rpm_put:
	pm_runtime_put(&client->dev);
err_unlock:
	mutex_unlock(&imx471->mutex);

	return ret;
}

static const struct v4l2_subdev_core_ops imx471_subdev_core_ops = {
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops imx471_video_ops = {
	.s_stream = imx471_set_stream,
};

static const struct v4l2_subdev_pad_ops imx471_pad_ops = {
	.enum_mbus_code = imx471_enum_mbus_code,
	.get_fmt = imx471_get_pad_format,
	.set_fmt = imx471_set_pad_format,
	.enum_frame_size = imx471_enum_frame_size,
};

static const struct v4l2_subdev_ops imx471_subdev_ops = {
	.core = &imx471_subdev_core_ops,
	.video = &imx471_video_ops,
	.pad = &imx471_pad_ops,
};

static const struct media_entity_operations imx471_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_subdev_internal_ops imx471_internal_ops = {
	.open = imx471_open,
};

/* Initialize control handlers */
static int imx471_init_controls(struct imx471 *imx471)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx471->sd);
	struct v4l2_ctrl_handler *ctrl_hdlr;
	s64 exposure_max;
	s64 vblank_def;
	s64 vblank_min;
	s64 hblank;
	u64 pixel_rate;
	const struct imx471_mode *mode;
	u32 max;
	int ret;

	ctrl_hdlr = &imx471->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 10);
	if (ret)
		return ret;

	ctrl_hdlr->lock = &imx471->mutex;
	max = ARRAY_SIZE(link_freq_menu_items) - 1;
	imx471->link_freq = v4l2_ctrl_new_int_menu(ctrl_hdlr, &imx471_ctrl_ops,
						   V4L2_CID_LINK_FREQ, max, 0,
						   link_freq_menu_items);
	if (imx471->link_freq)
		imx471->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* pixel_rate = link_freq * 2 * nr_of_lanes / bits_per_sample */
	pixel_rate = IMX471_LINK_FREQ_DEFAULT * 2 * 4;
	do_div(pixel_rate, 10);
	/* By default, PIXEL_RATE is read only */
	imx471->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &imx471_ctrl_ops,
					       V4L2_CID_PIXEL_RATE, pixel_rate,
					       pixel_rate, 1, pixel_rate);

	/* Initial vblank/hblank/exposure parameters based on current mode */
	mode = imx471->cur_mode;
	vblank_def = mode->fll_def - mode->height;
	vblank_min = mode->fll_min - mode->height;
	imx471->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &imx471_ctrl_ops,
					   V4L2_CID_VBLANK, vblank_min,
					   IMX471_FLL_MAX - mode->height,
					   1, vblank_def);

	hblank = mode->llp - mode->width;
	imx471->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &imx471_ctrl_ops,
					   V4L2_CID_HBLANK, hblank, hblank,
					   1, hblank);
	if (imx471->hblank)
		imx471->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* fll >= exposure time + adjust parameter (default value is 18) */
	exposure_max = mode->fll_def - 18;
	imx471->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &imx471_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     IMX471_EXPOSURE_MIN, exposure_max,
					     IMX471_EXPOSURE_STEP,
					     IMX471_EXPOSURE_DEFAULT);

	v4l2_ctrl_new_std(ctrl_hdlr, &imx471_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  IMX471_ANA_GAIN_MIN, IMX471_ANA_GAIN_MAX,
			  IMX471_ANA_GAIN_STEP, IMX471_ANA_GAIN_DEFAULT);

	/* Digital gain */
	v4l2_ctrl_new_std(ctrl_hdlr, &imx471_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  IMX471_DGTL_GAIN_MIN, IMX471_DGTL_GAIN_MAX,
			  IMX471_DGTL_GAIN_STEP, IMX471_DGTL_GAIN_DEFAULT);

	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &imx471_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(imx471_test_pattern_menu) - 1,
				     0, 0, imx471_test_pattern_menu);
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&client->dev, "control init failed: %d", ret);
		goto error;
	}

	imx471->sd.ctrl_handler = ctrl_hdlr;

	return 0;

error:
	v4l2_ctrl_handler_free(ctrl_hdlr);

	return ret;
}

static int imx471_check_hwcfg(struct device *dev)
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

        if (ext_clk != IMX471_EXT_CLK) {
                dev_err(dev, "external clock %d is not supported",
                        ext_clk);
                return -EINVAL;
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


static int imx471_probe(struct i2c_client *client)
{
	struct imx471 *imx471;
	bool full_power;
	int ret;


	/* Check HW config */
        ret = imx471_check_hwcfg(&client->dev);
        if (ret) {
                dev_err(&client->dev, "failed to check hwcfg: %d", ret);
                return ret;
        }

	imx471 = devm_kzalloc(&client->dev, sizeof(*imx471), GFP_KERNEL);
	if (!imx471)
		return -ENOMEM;

	mutex_init(&imx471->mutex);

	/* Initialize subdev */
	v4l2_i2c_subdev_init(&imx471->sd, client, &imx471_subdev_ops);

	full_power = acpi_dev_state_d0(&client->dev);
	if (full_power) {
		/* Check module identity */
		ret = imx471_identify_module(imx471);
		if (ret) {
			dev_err(&client->dev, "failed to find sensor: %d", ret);
			goto error_probe;
		}
	}

	/* Set default mode to max resolution */
	imx471->cur_mode = &supported_modes[0];

	ret = imx471_init_controls(imx471);
	if (ret) {
		dev_err(&client->dev, "failed to init controls: %d", ret);
		goto error_probe;
	}

	/* Initialize subdev */
	imx471->sd.internal_ops = &imx471_internal_ops;
	imx471->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		V4L2_SUBDEV_FL_HAS_EVENTS;
	imx471->sd.entity.ops = &imx471_subdev_entity_ops;
	imx471->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pad */
	imx471->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&imx471->sd.entity, 1, &imx471->pad);
	if (ret) {
		dev_err(&client->dev, "failed to init entity pads: %d", ret);
		goto error_handler_free;
	}

	/* Set the device's state to active if it's in D0 state. */
	if (full_power)
		pm_runtime_set_active(&client->dev);
	pm_runtime_enable(&client->dev);
	pm_runtime_idle(&client->dev);

	ret = v4l2_async_register_subdev_sensor(&imx471->sd);
	if (ret < 0)
		goto error_media_entity_pm;

	return 0;

error_media_entity_pm:
	pm_runtime_disable(&client->dev);
	pm_runtime_set_suspended(&client->dev);
	media_entity_cleanup(&imx471->sd.entity);

error_handler_free:
	v4l2_ctrl_handler_free(imx471->sd.ctrl_handler);

error_probe:
	mutex_destroy(&imx471->mutex);

	return ret;
}

static void imx471_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx471 *imx471 = to_imx471(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);

	pm_runtime_disable(&client->dev);
	pm_runtime_set_suspended(&client->dev);

	mutex_destroy(&imx471->mutex);
}

static const struct acpi_device_id imx471_acpi_ids[] __maybe_unused = {
	{ "SONY471A" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(acpi, imx471_acpi_ids);

static struct i2c_driver imx471_i2c_driver = {
	.driver = {
		.name = "imx471",
		.acpi_match_table = ACPI_PTR(imx471_acpi_ids),
	},
	.probe = imx471_probe,
	.remove = imx471_remove,
	.flags = I2C_DRV_ACPI_WAIVE_D0_PROBE,
};
module_i2c_driver(imx471_i2c_driver);

MODULE_AUTHOR("Jimmy Su <jimmy.su@intel.com>");
MODULE_DESCRIPTION("Sony imx471 sensor driver");
MODULE_LICENSE("GPL v2");
