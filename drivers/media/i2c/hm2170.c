// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 Intel Corporation.

#include <asm/unaligned.h>
#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/nvmem-provider.h>
#include <linux/regmap.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#if IS_ENABLED(CONFIG_INTEL_VSC)
#include <linux/vsc.h>
#endif

#define HM2170_LINK_FREQ_384MHZ		384000000ULL
#define HM2170_SCLK			76000000LL
#define HM2170_MCLK			19200000
#define HM2170_DATA_LANES		2
#define HM2170_RGB_DEPTH		10

#define HM2170_REG_CHIP_ID		0x0000
#define HM2170_CHIP_ID			0x2170

#define HM2170_REG_MODE_SELECT		0x0100
#define HM2170_MODE_STANDBY		0x00
#define HM2170_MODE_STREAMING		0x01

/* vertical-timings from sensor */
#define HM2170_REG_VTS			0x4809
#define HM2170_VTS_DEF			0x0484
#define HM2170_VTS_MIN			0x0484
#define HM2170_VTS_MAX			0x7fff

/* horizontal-timings from sensor */
#define HM2170_REG_HTS			0x480B

/* Exposure controls from sensor */
#define HM2170_REG_EXPOSURE		0x0202
#define HM2170_EXPOSURE_MIN		2
#define HM2170_EXPOSURE_MAX_MARGIN	2
#define HM2170_EXPOSURE_STEP		1

/* Analog gain controls from sensor */
#define HM2170_REG_ANALOG_GAIN		0x0208
#define HM2170_ANAL_GAIN_MIN		0
#define HM2170_ANAL_GAIN_MAX		80
#define HM2170_ANAL_GAIN_STEP		1

/* Digital gain controls from sensor */
#define HM2170_REG_DIGITAL_GAIN		0x020A
#define HM2170_DGTL_GAIN_MIN		256
#define HM2170_DGTL_GAIN_MAX		1020
#define HM2170_DGTL_GAIN_STEP		1
#define HM2170_DGTL_GAIN_DEFAULT	256

/* Register update control */
#define HM2170_REG_COMMAND_UPDATE	0x0104
#define HM2170_COMMAND_UPDATE		0x00
#define HM2170_COMMAND_HOLD		0x01

/* Test Pattern Control */
#define HM2170_REG_TEST_PATTERN		0x0601
#define HM2170_TEST_PATTERN_ENABLE	BIT(0)
#define HM2170_TEST_PATTERN_BAR_SHIFT	1

enum {
	HM2170_LINK_FREQ_384MHZ_INDEX,
};

struct hm2170_reg {
	u16 address;
	u8 val;
};

struct hm2170_reg_list {
	u32 num_of_regs;
	const struct hm2170_reg *regs;
};

struct hm2170_link_freq_config {
	const struct hm2170_reg_list reg_list;
};

struct hm2170_mode {
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
	const struct hm2170_reg_list reg_list;
};

static const struct hm2170_reg mode_1928x1088_regs[] = {
	{0x0103, 0x00},
	{0x0202, 0x03},
	{0x0203, 0x60},
	{0x0300, 0x5E},
	{0x0301, 0x3F},
	{0x0302, 0x07},
	{0x0303, 0x04},
	{0x1000, 0xC3},
	{0x1001, 0xC0},
	{0x2000, 0x00},
	{0x2088, 0x01},
	{0x2089, 0x00},
	{0x208A, 0xC8},
	{0x2700, 0x00},
	{0x2711, 0x01},
	{0x2713, 0x04},
	{0x272F, 0x01},
	{0x2800, 0x01},
	{0x2821, 0x8E},
	{0x2823, 0x01},
	{0x282E, 0x01},
	{0x282F, 0xC0},
	{0x2839, 0x13},
	{0x283A, 0x01},
	{0x283B, 0x0F},
	{0x2842, 0x0C},
	{0x2846, 0x01},
	{0x2847, 0x94},
	{0x3001, 0x00},
	{0x3002, 0x88},
	{0x3004, 0x02},
	{0x3024, 0x20},
	{0x3025, 0x12},
	{0x3026, 0x00},
	{0x3027, 0x81},
	{0x3028, 0x01},
	{0x3029, 0x00},
	{0x302A, 0x30},
	{0x3042, 0x00},
	{0x3070, 0x01},
	{0x30C4, 0x20},
	{0x30D0, 0x01},
	{0x30D2, 0x8E},
	{0x30D7, 0x02},
	{0x30D9, 0x9E},
	{0x30DE, 0x03},
	{0x30E0, 0x9E},
	{0x30E5, 0x04},
	{0x30E7, 0x9F},
	{0x30EC, 0x24},
	{0x30EE, 0x9F},
	{0x30F3, 0x44},
	{0x30F5, 0x9F},
	{0x30F8, 0x00},
	{0x3101, 0x02},
	{0x3103, 0x9E},
	{0x3108, 0x03},
	{0x310A, 0x9E},
	{0x310F, 0x04},
	{0x3111, 0x9E},
	{0x3116, 0x24},
	{0x3118, 0x9F},
	{0x311D, 0x44},
	{0x311F, 0x9F},
	{0x3124, 0x64},
	{0x3126, 0x9F},
	{0x3135, 0x01},
	{0x3137, 0x03},
	{0x313C, 0x52},
	{0x313E, 0x68},
	{0x3144, 0x3E},
	{0x3145, 0x68},
	{0x3146, 0x08},
	{0x3147, 0x03},
	{0x3148, 0x0F},
	{0x3149, 0xFF},
	{0x314A, 0x13},
	{0x314B, 0x0F},
	{0x314C, 0xF8},
	{0x314D, 0x04},
	{0x314E, 0x10},
	{0x3161, 0x11},
	{0x3171, 0x05},
	{0x317A, 0x21},
	{0x317B, 0xF0},
	{0x317C, 0x07},
	{0x317D, 0x09},
	{0x3183, 0x18},
	{0x3184, 0x4A},
	{0x318E, 0x88},
	{0x318F, 0x00},
	{0x3190, 0x00},
	{0x4003, 0x02},
	{0x4004, 0x02},
	{0x4800, 0x26},
	{0x4801, 0x10},
	{0x4802, 0x00},
	{0x4803, 0x00},
	{0x4804, 0x7F},
	{0x4805, 0x7F},
	{0x4806, 0x3F},
	{0x4807, 0x1F},
	{0x4809, 0x04},
	{0x480A, 0x84},
	{0x480B, 0x08},
	{0x480C, 0x90},
	{0x480D, 0x00},
	{0x480E, 0x01},
	{0x480F, 0x04},
	{0x4810, 0x40},
	{0x4811, 0x00},
	{0x4812, 0x00},
	{0x4813, 0x00},
	{0x4814, 0x00},
	{0x4815, 0x00},
	{0x4816, 0x00},
	{0x4817, 0x00},
	{0x4818, 0x00},
	{0x4819, 0x03},
	{0x481F, 0x00},
	{0x4820, 0x0E},
	{0x4821, 0x0E},
	{0x4840, 0x00},
	{0x4844, 0x00},
	{0x4845, 0x00},
	{0x4846, 0x00},
	{0x4847, 0x00},
	{0x4848, 0x00},
	{0x4849, 0xF1},
	{0x484A, 0x00},
	{0x484B, 0x88},
	{0x484C, 0x01},
	{0x484D, 0x04},
	{0x484E, 0x64},
	{0x484F, 0x50},
	{0x4850, 0x04},
	{0x4851, 0x00},
	{0x4852, 0x01},
	{0x4853, 0x19},
	{0x4854, 0x50},
	{0x4855, 0x04},
	{0x4856, 0x00},
	{0x4863, 0x02},
	{0x4864, 0x3D},
	{0x4865, 0x02},
	{0x4866, 0xB0},
	{0x4880, 0x00},
	{0x48A0, 0x00},
	{0x48A1, 0x04},
	{0x48A2, 0x01},
	{0x48A3, 0xDD},
	{0x48A4, 0x0C},
	{0x48A5, 0x3B},
	{0x48A6, 0x20},
	{0x48A7, 0x20},
	{0x48A8, 0x20},
	{0x48A9, 0x20},
	{0x48AA, 0x00},
	{0x48C0, 0x3F},
	{0x48C1, 0x29},
	{0x48C3, 0x14},
	{0x48C4, 0x00},
	{0x48C5, 0x07},
	{0x48C6, 0x88},
	{0x48C7, 0x04},
	{0x48C8, 0x40},
	{0x48C9, 0x00},
	{0x48CA, 0x00},
	{0x48CB, 0x00},
	{0x48CC, 0x00},
	{0x48F0, 0x00},
	{0x48F1, 0x00},
	{0x48F2, 0x04},
	{0x48F3, 0x01},
	{0x48F4, 0xE0},
	{0x48F5, 0x01},
	{0x48F6, 0x10},
	{0x48F7, 0x00},
	{0x48F8, 0x00},
	{0x48F9, 0x00},
	{0x48FA, 0x00},
	{0x48FB, 0x01},
	{0x4931, 0x2B},
	{0x4932, 0x01},
	{0x4933, 0x01},
	{0x4934, 0x00},
	{0x4935, 0x0F},
	{0x4980, 0x00},
	{0x4A72, 0x01},
	{0x4A73, 0x01},
	{0x4C30, 0x00},
	{0x4CF2, 0x01},
	{0x4CF3, 0x01},
	{0x0104, 0x00},
};

static const char * const hm2170_test_pattern_menu[] = {
	"Disabled",
	"Solid Color",
	"Color Bar",
	"Color Bar With Blending",
	"PN11",
};

static const s64 link_freq_menu_items[] = {
	HM2170_LINK_FREQ_384MHZ,
};

static const struct hm2170_mode supported_modes[] = {
	{
		.width = 1928,
		.height = 1088,
		.hts = 2192,
		.vts_def = HM2170_VTS_DEF,
		.vts_min = HM2170_VTS_MIN,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1928x1088_regs),
			.regs = mode_1928x1088_regs,
		},
		.link_freq_index = HM2170_LINK_FREQ_384MHZ_INDEX,
	},
};

struct hm2170 {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;

	/* V4L2 Controls */
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *exposure;
#if IS_ENABLED(CONFIG_INTEL_VSC)
	struct v4l2_ctrl *privacy_status;
#endif
	/* Current mode */
	const struct hm2170_mode *cur_mode;

	/* To serialize asynchronus callbacks */
	struct mutex mutex;

	/* Streaming on/off */
	bool streaming;
};

static inline struct hm2170 *to_hm2170(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct hm2170, sd);
}

static u64 to_pixel_rate(u32 f_index)
{
	u64 pixel_rate = link_freq_menu_items[f_index] * 2 * HM2170_DATA_LANES;

	do_div(pixel_rate, HM2170_RGB_DEPTH);

	return pixel_rate;
}

static u64 to_pixels_per_line(u32 hts, u32 f_index)
{
	u64 ppl = hts * to_pixel_rate(f_index);

	do_div(ppl, HM2170_SCLK);

	return ppl;
}

static int hm2170_read_reg(struct hm2170 *hm2170, u16 reg, u16 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&hm2170->sd);
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

static int hm2170_write_reg(struct hm2170 *hm2170, u16 reg, u16 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&hm2170->sd);
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

static int hm2170_write_reg_list(struct hm2170 *hm2170,
				 const struct hm2170_reg_list *r_list)
{
	struct i2c_client *client = v4l2_get_subdevdata(&hm2170->sd);
	unsigned int i;
	int ret = 0;

	for (i = 0; i < r_list->num_of_regs; i++) {
		ret = hm2170_write_reg(hm2170, r_list->regs[i].address, 1,
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

static int hm2170_test_pattern(struct hm2170 *hm2170, u32 pattern)
{
	if (pattern)
		pattern = pattern << HM2170_TEST_PATTERN_BAR_SHIFT |
			  HM2170_TEST_PATTERN_ENABLE;

	return hm2170_write_reg(hm2170, HM2170_REG_TEST_PATTERN, 1, pattern);
}

static int hm2170_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct hm2170 *hm2170 = container_of(ctrl->handler,
					     struct hm2170, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&hm2170->sd);
	s64 exposure_max;
	int ret = 0;

	/* Propagate change of current control to all related controls */
	if (ctrl->id == V4L2_CID_VBLANK) {
		/* Update max exposure while meeting expected vblanking */
		exposure_max = hm2170->cur_mode->height + ctrl->val -
			       HM2170_EXPOSURE_MAX_MARGIN;
		__v4l2_ctrl_modify_range(hm2170->exposure,
					 hm2170->exposure->minimum,
					 exposure_max, hm2170->exposure->step,
					 exposure_max);
	}

	/* V4L2 controls values will be applied only when power is already up */
	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	ret = hm2170_write_reg(hm2170, HM2170_REG_COMMAND_UPDATE, 1,
			       HM2170_COMMAND_HOLD);
	if (ret)
		dev_dbg(&client->dev, "failed to hold command");

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		ret = hm2170_write_reg(hm2170, HM2170_REG_ANALOG_GAIN, 1,
				       ctrl->val);
		break;

	case V4L2_CID_DIGITAL_GAIN:
		ret = hm2170_write_reg(hm2170, HM2170_REG_DIGITAL_GAIN, 2,
						ctrl->val);
		break;

	case V4L2_CID_EXPOSURE:
		ret = hm2170_write_reg(hm2170, HM2170_REG_EXPOSURE, 2, ctrl->val);
		break;

	case V4L2_CID_VBLANK:
		ret = hm2170_write_reg(hm2170, HM2170_REG_VTS, 2,
				       hm2170->cur_mode->height + ctrl->val);
		break;

	case V4L2_CID_TEST_PATTERN:
		ret = hm2170_test_pattern(hm2170, ctrl->val);
		break;

#if IS_ENABLED(CONFIG_INTEL_VSC)
	case V4L2_CID_PRIVACY:
		dev_dbg(&client->dev, "set privacy to %d", ctrl->val);
		break;
#endif

	default:
		ret = -EINVAL;
		break;
	}
	ret |= hm2170_write_reg(hm2170, HM2170_REG_COMMAND_UPDATE, 1,
				HM2170_COMMAND_UPDATE);

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops hm2170_ctrl_ops = {
	.s_ctrl = hm2170_set_ctrl,
};

static int hm2170_init_controls(struct hm2170 *hm2170)
{
	struct v4l2_ctrl_handler *ctrl_hdlr;
	const struct hm2170_mode *cur_mode;
	s64 exposure_max, h_blank, pixel_rate;
	u32 vblank_min, vblank_max, vblank_default;
	int size;
	int ret = 0;

	ctrl_hdlr = &hm2170->ctrl_handler;
#if IS_ENABLED(CONFIG_INTEL_VSC)
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 9);
#else
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 8);
#endif
	if (ret)
		return ret;

	ctrl_hdlr->lock = &hm2170->mutex;
	cur_mode = hm2170->cur_mode;
	size = ARRAY_SIZE(link_freq_menu_items);

	hm2170->link_freq = v4l2_ctrl_new_int_menu(ctrl_hdlr, &hm2170_ctrl_ops,
						   V4L2_CID_LINK_FREQ,
						   size - 1, 0,
						   link_freq_menu_items);
	if (hm2170->link_freq)
		hm2170->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	pixel_rate = to_pixel_rate(HM2170_LINK_FREQ_384MHZ_INDEX);
	hm2170->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &hm2170_ctrl_ops,
					       V4L2_CID_PIXEL_RATE, 0,
					       pixel_rate, 1, pixel_rate);

	vblank_min = cur_mode->vts_min - cur_mode->height;
	vblank_max = HM2170_VTS_MAX - cur_mode->height;
	vblank_default = cur_mode->vts_def - cur_mode->height;
	hm2170->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &hm2170_ctrl_ops,
					   V4L2_CID_VBLANK, vblank_min,
					   vblank_max, 1, vblank_default);

	h_blank = to_pixels_per_line(cur_mode->hts, cur_mode->link_freq_index);
	h_blank -= cur_mode->width;
	hm2170->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &hm2170_ctrl_ops,
					   V4L2_CID_HBLANK, h_blank, h_blank, 1,
					   h_blank);
	if (hm2170->hblank)
		hm2170->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
#if IS_ENABLED(CONFIG_INTEL_VSC)
	hm2170->privacy_status = v4l2_ctrl_new_std(ctrl_hdlr, &hm2170_ctrl_ops,
								V4L2_CID_PRIVACY, 0, 1, 1, 0);
#endif

	v4l2_ctrl_new_std(ctrl_hdlr, &hm2170_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  HM2170_ANAL_GAIN_MIN, HM2170_ANAL_GAIN_MAX,
			  HM2170_ANAL_GAIN_STEP, HM2170_ANAL_GAIN_MIN);
	v4l2_ctrl_new_std(ctrl_hdlr, &hm2170_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  HM2170_DGTL_GAIN_MIN, HM2170_DGTL_GAIN_MAX,
			  HM2170_DGTL_GAIN_STEP, HM2170_DGTL_GAIN_DEFAULT);
	exposure_max = cur_mode->vts_def - HM2170_EXPOSURE_MAX_MARGIN;
	hm2170->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &hm2170_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     HM2170_EXPOSURE_MIN, exposure_max,
					     HM2170_EXPOSURE_STEP,
					     exposure_max);
	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &hm2170_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(hm2170_test_pattern_menu) - 1,
				     0, 0, hm2170_test_pattern_menu);
	if (ctrl_hdlr->error)
		return ctrl_hdlr->error;

	hm2170->sd.ctrl_handler = ctrl_hdlr;

	return 0;
}

static void hm2170_update_pad_format(const struct hm2170_mode *mode,
				     struct v4l2_mbus_framefmt *fmt)
{
	fmt->width = mode->width;
	fmt->height = mode->height;
	fmt->code = MEDIA_BUS_FMT_SGRBG10_1X10;
	fmt->field = V4L2_FIELD_NONE;
}

#if IS_ENABLED(CONFIG_INTEL_VSC)
static void hm2170_vsc_privacy_callback(void *handle,
					enum vsc_privacy_status status)
{
	struct hm2170 *hm2170 = handle;

	v4l2_ctrl_s_ctrl(hm2170->privacy_status, !status);
}
#endif

static int hm2170_start_streaming(struct hm2170 *hm2170)
{
	struct i2c_client *client = v4l2_get_subdevdata(&hm2170->sd);
	const struct hm2170_reg_list *reg_list;
	int ret = 0;
#if IS_ENABLED(CONFIG_INTEL_VSC)
	struct vsc_mipi_config conf;
	struct vsc_camera_status status;

	conf.lane_num = HM2170_DATA_LANES;
	/* frequency unit 100k */
	conf.freq = HM2170_LINK_FREQ_384MHZ / 100000;
	ret = vsc_acquire_camera_sensor(&conf, hm2170_vsc_privacy_callback,
					hm2170, &status);
	if (ret) {
		dev_err(&client->dev, "Acquire VSC failed");
		return ret;
	}
	__v4l2_ctrl_s_ctrl(hm2170->privacy_status, !(status.status));
#endif
	reg_list = &hm2170->cur_mode->reg_list;
	ret = hm2170_write_reg_list(hm2170, reg_list);
	if (ret) {
		dev_err(&client->dev, "failed to set mode");
		return ret;
	}

	ret = __v4l2_ctrl_handler_setup(hm2170->sd.ctrl_handler);
	if (ret)
		return ret;

	ret = hm2170_write_reg(hm2170, HM2170_REG_MODE_SELECT, 1,
			       HM2170_MODE_STREAMING);
	if (ret)
		dev_err(&client->dev, "failed to start streaming");

	return ret;
}

static void hm2170_stop_streaming(struct hm2170 *hm2170)
{
	struct i2c_client *client = v4l2_get_subdevdata(&hm2170->sd);
#if IS_ENABLED(CONFIG_INTEL_VSC)
	struct vsc_camera_status status;
#endif

	if (hm2170_write_reg(hm2170, HM2170_REG_MODE_SELECT, 1,
			     HM2170_MODE_STANDBY))
		dev_err(&client->dev, "failed to stop streaming");
#if IS_ENABLED(CONFIG_INTEL_VSC)
	if (vsc_release_camera_sensor(&status))
		dev_err(&client->dev, "Release VSC failed");
#endif
}

static int hm2170_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct hm2170 *hm2170 = to_hm2170(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (hm2170->streaming == enable)
		return 0;

	mutex_lock(&hm2170->mutex);
	if (enable) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			mutex_unlock(&hm2170->mutex);
			return ret;
		}

		ret = hm2170_start_streaming(hm2170);
		if (ret) {
			enable = 0;
			hm2170_stop_streaming(hm2170);
			pm_runtime_put(&client->dev);
		}
	} else {
		hm2170_stop_streaming(hm2170);
		pm_runtime_put(&client->dev);
	}

	hm2170->streaming = enable;
	mutex_unlock(&hm2170->mutex);

	return ret;
}

static int __maybe_unused hm2170_suspend(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct hm2170 *hm2170 = to_hm2170(sd);

	mutex_lock(&hm2170->mutex);
	if (hm2170->streaming)
		hm2170_stop_streaming(hm2170);

	mutex_unlock(&hm2170->mutex);

	return 0;
}

static int __maybe_unused hm2170_resume(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct hm2170 *hm2170 = to_hm2170(sd);
	int ret = 0;

	mutex_lock(&hm2170->mutex);
	if (!hm2170->streaming)
		goto exit;

	ret = hm2170_start_streaming(hm2170);
	if (ret) {
		hm2170->streaming = false;
		hm2170_stop_streaming(hm2170);
	}

exit:
	mutex_unlock(&hm2170->mutex);
	return ret;
}

static int hm2170_set_format(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *sd_state,
			     struct v4l2_subdev_format *fmt)
{
	struct hm2170 *hm2170 = to_hm2170(sd);
	const struct hm2170_mode *mode;
	s32 vblank_def, h_blank;

	mode = v4l2_find_nearest_size(supported_modes,
				      ARRAY_SIZE(supported_modes), width,
				      height, fmt->format.width,
				      fmt->format.height);

	mutex_lock(&hm2170->mutex);
	hm2170_update_pad_format(mode, &fmt->format);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
	} else {
		hm2170->cur_mode = mode;
		__v4l2_ctrl_s_ctrl(hm2170->link_freq, mode->link_freq_index);
		__v4l2_ctrl_s_ctrl_int64(hm2170->pixel_rate,
					 to_pixel_rate(mode->link_freq_index));

		/* Update limits and set FPS to default */
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(hm2170->vblank,
					 mode->vts_min - mode->height,
					 HM2170_VTS_MAX - mode->height, 1,
					 vblank_def);
		__v4l2_ctrl_s_ctrl(hm2170->vblank, vblank_def);
		h_blank = to_pixels_per_line(mode->hts, mode->link_freq_index) -
			  mode->width;
		__v4l2_ctrl_modify_range(hm2170->hblank, h_blank, h_blank, 1,
					 h_blank);
	}
	mutex_unlock(&hm2170->mutex);

	return 0;
}

static int hm2170_get_format(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *sd_state,
			     struct v4l2_subdev_format *fmt)
{
	struct hm2170 *hm2170 = to_hm2170(sd);

	mutex_lock(&hm2170->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		fmt->format = *v4l2_subdev_get_try_format(&hm2170->sd,
							  sd_state, fmt->pad);
	else
		hm2170_update_pad_format(hm2170->cur_mode, &fmt->format);

	mutex_unlock(&hm2170->mutex);

	return 0;
}

static int hm2170_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index > 0)
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_SGRBG10_1X10;

	return 0;
}

static int hm2170_enum_frame_size(struct v4l2_subdev *sd,
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

static int hm2170_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct hm2170 *hm2170 = to_hm2170(sd);

	mutex_lock(&hm2170->mutex);
	hm2170_update_pad_format(&supported_modes[0],
				 v4l2_subdev_get_try_format(sd, fh->state, 0));
	mutex_unlock(&hm2170->mutex);

	return 0;
}

static const struct v4l2_subdev_video_ops hm2170_video_ops = {
	.s_stream = hm2170_set_stream,
};

static const struct v4l2_subdev_pad_ops hm2170_pad_ops = {
	.set_fmt = hm2170_set_format,
	.get_fmt = hm2170_get_format,
	.enum_mbus_code = hm2170_enum_mbus_code,
	.enum_frame_size = hm2170_enum_frame_size,
};

static const struct v4l2_subdev_ops hm2170_subdev_ops = {
	.video = &hm2170_video_ops,
	.pad = &hm2170_pad_ops,
};

static const struct media_entity_operations hm2170_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_subdev_internal_ops hm2170_internal_ops = {
	.open = hm2170_open,
};

static int hm2170_identify_module(struct hm2170 *hm2170)
{
	struct i2c_client *client = v4l2_get_subdevdata(&hm2170->sd);
	int ret;
	u32 val;

	ret = hm2170_read_reg(hm2170, HM2170_REG_CHIP_ID, 2, &val);

	if (ret)
		return ret;

	if (val != HM2170_CHIP_ID) {
		dev_err(&client->dev, "chip id mismatch: %x!=%x",
			HM2170_CHIP_ID, val);
		return -ENXIO;
	}

	return 0;
}

static int hm2170_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct hm2170 *hm2170 = to_hm2170(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);
	pm_runtime_disable(&client->dev);
	mutex_destroy(&hm2170->mutex);

	return 0;
}

static int hm2170_probe(struct i2c_client *client)
{
	struct hm2170 *hm2170;
	int ret = 0;
#if IS_ENABLED(CONFIG_INTEL_VSC)
	struct vsc_mipi_config conf;
	struct vsc_camera_status status;
#endif

#if IS_ENABLED(CONFIG_INTEL_VSC)
	conf.lane_num = HM2170_DATA_LANES;
	/* frequency unit 100k */
	conf.freq = HM2170_LINK_FREQ_384MHZ / 100000;
	ret = vsc_acquire_camera_sensor(&conf, NULL, NULL, &status);
	if (ret == -EAGAIN) {
		dev_dbg(&client->dev, "VSC not ready, will re-probe");
		return -EPROBE_DEFER;
	} else if (ret) {
		dev_err(&client->dev, "Acquire VSC failed");
		return ret;
	}
#endif
	hm2170 = devm_kzalloc(&client->dev, sizeof(*hm2170), GFP_KERNEL);
	if (!hm2170) {
		ret = -ENOMEM;
		goto probe_error_ret;
	}

	v4l2_i2c_subdev_init(&hm2170->sd, client, &hm2170_subdev_ops);
	ret = hm2170_identify_module(hm2170);
	if (ret) {
		dev_err(&client->dev, "failed to find sensor: %d", ret);
		goto probe_error_ret;
	}

	mutex_init(&hm2170->mutex);
	hm2170->cur_mode = &supported_modes[0];
	ret = hm2170_init_controls(hm2170);
	if (ret) {
		dev_err(&client->dev, "failed to init controls: %d", ret);
		goto probe_error_v4l2_ctrl_handler_free;
	}

	hm2170->sd.internal_ops = &hm2170_internal_ops;
	hm2170->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	hm2170->sd.entity.ops = &hm2170_subdev_entity_ops;
	hm2170->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	hm2170->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&hm2170->sd.entity, 1, &hm2170->pad);
	if (ret) {
		dev_err(&client->dev, "failed to init entity pads: %d", ret);
		goto probe_error_v4l2_ctrl_handler_free;
	}

	ret = v4l2_async_register_subdev_sensor(&hm2170->sd);
	if (ret < 0) {
		dev_err(&client->dev, "failed to register V4L2 subdev: %d",
			ret);
		goto probe_error_media_entity_cleanup;
	}

#if IS_ENABLED(CONFIG_INTEL_VSC)
	vsc_release_camera_sensor(&status);
#endif
	/*
	 * Device is already turned on by i2c-core with ACPI domain PM.
	 * Enable runtime PM and turn off the device.
	 */
	pm_runtime_set_active(&client->dev);
	pm_runtime_enable(&client->dev);
	pm_runtime_idle(&client->dev);

	return 0;

probe_error_media_entity_cleanup:
	media_entity_cleanup(&hm2170->sd.entity);

probe_error_v4l2_ctrl_handler_free:
	v4l2_ctrl_handler_free(hm2170->sd.ctrl_handler);
	mutex_destroy(&hm2170->mutex);

probe_error_ret:
#if IS_ENABLED(CONFIG_INTEL_VSC)
	vsc_release_camera_sensor(&status);
#endif
	return ret;
}

static const struct dev_pm_ops hm2170_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(hm2170_suspend, hm2170_resume)
};

static const struct acpi_device_id hm2170_acpi_ids[] = {
	{"HIMX2170"},
	{}
};

MODULE_DEVICE_TABLE(acpi, hm2170_acpi_ids);

static struct i2c_driver hm2170_i2c_driver = {
	.driver = {
		.name = "hm2170",
		.pm = &hm2170_pm_ops,
		.acpi_match_table = hm2170_acpi_ids,
	},
	.probe_new = hm2170_probe,
	.remove = hm2170_remove,
};

module_i2c_driver(hm2170_i2c_driver);

MODULE_AUTHOR("Shawn Tu <shawnx.tu@intel.com>");
MODULE_DESCRIPTION("Himax HM2170 sensor driver");
MODULE_LICENSE("GPL v2");
