// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2023 Intel Corporation.

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

#define OV02E10_LINK_FREQ_360MHZ         360000000ULL
#define OV02E10_SCLK                     36000000LL
#define OV02E10_MCLK                     19200000
#define OV02E10_DATA_LANES               2
#define OV02E10_RGB_DEPTH                10

#define OV02E10_REG_DELAY                0xff

#define OV02E10_REG_PAGE_FLAG            0xfd
#define OV02E10_PAGE_0                   0x0
#define OV02E10_PAGE_1                   0x1
#define OV02E10_PAGE_2                   0x2
#define OV02E10_PAGE_3                   0x3
#define OV02E10_PAGE_5                   0x4
#define OV02E10_PAGE_7                   0x5
#define OV02E10_PAGE_8                   0x6
#define OV02E10_PAGE_9                   0xF
#define OV02E10_PAGE_D                   0x8
#define OV02E10_PAGE_E                   0x9
#define OV02E10_PAGE_F                   0xA

#define OV02E10_REG_CHIP_ID              0x00
#define OV02E10_CHIP_ID                  0x45025610

/* vertical-timings from sensor */
#define OV02E10_REG_VTS                  0x35
#define OV02E10_VTS_DEF                  2244
#define OV02E10_VTS_MIN                  2244
#define OV02E10_VTS_MAX                  0x7fff

/* horizontal-timings from sensor */
#define OV02E10_REG_HTS                  0x37

/* Exposure controls from sensor */
#define OV02E10_REG_EXPOSURE             0x03
#define OV02E10_EXPOSURE_MIN             1
#define OV02E10_EXPOSURE_MAX_MARGIN      2
#define OV02E10_EXPOSURE_STEP            1

/* Analog gain controls from sensor */
#define OV02E10_REG_ANALOG_GAIN          0x24
#define OV02E10_ANAL_GAIN_MIN            0x10
#define OV02E10_ANAL_GAIN_MAX            0xf8
#define OV02E10_ANAL_GAIN_STEP           1

/* Digital gain controls from sensor */
#define OV02E10_REG_DIGITAL_GAIN         0x21
#define OV02E10_DGTL_GAIN_MIN            256
#define OV02E10_DGTL_GAIN_MAX            1020
#define OV02E10_DGTL_GAIN_STEP           1
#define OV02E10_DGTL_GAIN_DEFAULT        256

/* Register update control */
#define OV02E10_REG_COMMAND_UPDATE       0xE7
#define OV02E10_COMMAND_UPDATE           0x00
#define OV02E10_COMMAND_HOLD             0x01

/* Test Pattern Control */
#define OV02E10_REG_TEST_PATTERN         0x12
#define OV02E10_TEST_PATTERN_ENABLE      BIT(0)
#define OV02E10_TEST_PATTERN_BAR_SHIFT   1

enum {
	OV02E10_LINK_FREQ_360MHZ_INDEX,
};

struct ov02e10_reg {
	u16 address;
	u8 val;
};

struct ov02e10_reg_list {
	u32 num_of_regs;
	const struct ov02e10_reg *regs;
};

struct ov02e10_link_freq_config {
	const struct ov02e10_reg_list reg_list;
};

struct ov02e10_mode {
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
	const struct ov02e10_reg_list reg_list;
};

static const struct ov02e10_reg ov02e10_standby[] = {
	{ OV02E10_REG_PAGE_FLAG, OV02E10_PAGE_0 },
	{ 0xa0, 0x00 },
	{ OV02E10_REG_PAGE_FLAG, OV02E10_PAGE_1 },
	{ 0x01, 0x02 }
};

static const struct ov02e10_reg_list ov02e10_standby_list = {
	.num_of_regs = ARRAY_SIZE(ov02e10_standby),
	.regs = ov02e10_standby
};

static const struct ov02e10_reg ov02e10_streaming[] = {
	{ OV02E10_REG_PAGE_FLAG, OV02E10_PAGE_0 },
	{ 0xa0, 0x01 },
	{ OV02E10_REG_PAGE_FLAG, OV02E10_PAGE_1 },
	{ 0x01, 0x02 }
};

static const struct ov02e10_reg_list ov02e10_streaming_list = {
	.num_of_regs = ARRAY_SIZE(ov02e10_streaming),
	.regs = ov02e10_streaming
};

static const struct ov02e10_reg mode_1928x1088_30fps_2lane[] = {
	{ 0xfd, 0x00 },
	{ 0x20, 0x00 },
	{ 0x20, 0x0b },
	{ 0x21, 0x02 },
	{ 0x10, 0x23 },
	{ 0xc5, 0x04 },
	{ 0x21, 0x00 },
	{ 0x14, 0x96 },
	{ 0x17, 0x01 },
	{ 0xfd, 0x01 },
	{ 0x03, 0x00 },
	{ 0x04, 0x04 },
	{ 0x05, 0x04 },
	{ 0x06, 0x62 },
	{ 0x07, 0x01 },
	{ 0x22, 0x80 },
	{ 0x24, 0xff },
	{ 0x40, 0xc6 },
	{ 0x41, 0x18 },
	{ 0x45, 0x3f },
	{ 0x48, 0x0c },
	{ 0x4c, 0x08 },
	{ 0x51, 0x12 },
	{ 0x52, 0x10 },
	{ 0x57, 0x98 },
	{ 0x59, 0x06 },
	{ 0x5a, 0x04 },
	{ 0x5c, 0x38 },
	{ 0x5e, 0x10 },
	{ 0x67, 0x11 },
	{ 0x7b, 0x04 },
	{ 0x81, 0x12 },
	{ 0x90, 0x51 },
	{ 0x91, 0x09 },
	{ 0x92, 0x21 },
	{ 0x93, 0x28 },
	{ 0x95, 0x54 },
	{ 0x9d, 0x20 },
	{ 0x9e, 0x04 },
	{ 0xb1, 0x9a },
	{ 0xb2, 0x86 },
	{ 0xb6, 0x3f },
	{ 0xb9, 0x30 },
	{ 0xc1, 0x01 },
	{ 0xc5, 0xa0 },
	{ 0xc6, 0x73 },
	{ 0xc7, 0x04 },
	{ 0xc8, 0x25 },
	{ 0xc9, 0x05 },
	{ 0xca, 0x28 },
	{ 0xcb, 0x00 },
	{ 0xcf, 0x16 },
	{ 0xd2, 0xd0 },
	{ 0xd7, 0x3f },
	{ 0xd8, 0x40 },
	{ 0xd9, 0x40 },
	{ 0xda, 0x44 },
	{ 0xdb, 0x3d },
	{ 0xdc, 0x3d },
	{ 0xdd, 0x3d },
	{ 0xde, 0x3d },
	{ 0xdf, 0xf0 },
	{ 0xea, 0x0f },
	{ 0xeb, 0x04 },
	{ 0xec, 0x29 },
	{ 0xee, 0x47 },
	{ 0xfd, 0x01 },
	{ 0x31, 0x01 },
	{ 0x27, 0x00 },
	{ 0x2f, 0x41 },
	{ 0xfd, 0x02 },
	{ 0xa1, 0x01 },
	{ 0xfd, 0x02 },
	{ 0x9a, 0x03 },
	{ 0xfd, 0x03 },
	{ 0x9d, 0x0f },
	{ 0xfd, 0x07 },
	{ 0x42, 0x00 },
	{ 0x43, 0xad },
	{ 0x44, 0x00 },
	{ 0x45, 0xa8 },
	{ 0x46, 0x00 },
	{ 0x47, 0xa8 },
	{ 0x48, 0x00 },
	{ 0x49, 0xad },
	{ 0xfd, 0x00 },
	{ 0xc4, 0x01 },
	{ 0xfd, 0x01 },
	{ 0x33, 0x03 },
	{ 0xfd, 0x00 },
	{ 0x20, 0x1f },
};

static const char *const ov02e10_test_pattern_menu[] = {
	"Disabled",
	"Color Bar",
};

static const s64 link_freq_menu_items[] = {
	OV02E10_LINK_FREQ_360MHZ,
};

static const struct ov02e10_mode supported_modes[] = {
	{
	 .width = 1928,
	 .height = 1088,
	 .hts = 534,
	 .vts_def = 2244,
	 .vts_min = 2244,
	 .reg_list = {
		      .num_of_regs = ARRAY_SIZE(mode_1928x1088_30fps_2lane),
		      .regs = mode_1928x1088_30fps_2lane,
		      },

	 .link_freq_index = OV02E10_LINK_FREQ_360MHZ_INDEX,
	  },
};

struct ov02e10 {
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
	const struct ov02e10_mode *cur_mode;

	/* To serialize asynchronus callbacks */
	struct mutex mutex;

	/* Streaming on/off */
	bool streaming;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
    IS_ENABLED(CONFIG_INTEL_VSC)
	bool use_intel_vsc;
#endif
};

static inline struct ov02e10 *to_ov02e10(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct ov02e10, sd);
}

static u64 to_pixel_rate(u32 f_index)
{
	u64 pixel_rate = link_freq_menu_items[f_index] * 2 * OV02E10_DATA_LANES;

	do_div(pixel_rate, OV02E10_RGB_DEPTH);

	return pixel_rate;
}

static u64 to_pixels_per_line(u32 hts, u32 f_index)
{
	u64 ppl = hts * to_pixel_rate(f_index);

	do_div(ppl, OV02E10_SCLK);

	return ppl;
}

static int ov02e10_read_reg(struct ov02e10 *ov02e10, u8 reg, u16 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov02e10->sd);
	struct i2c_msg msgs[2];
	u8 data_buf[4] = { 0 };
	int ret;

	if (len > sizeof(data_buf))
		return -EINVAL;

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg;
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

static int ov02e10_write_reg(struct ov02e10 *ov02e10, u8 reg, u16 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov02e10->sd);
	u8 buf[5];
	int ret;

	if (len > 4)
		return -EINVAL;

	if (reg == OV02E10_REG_DELAY) {
		msleep(val);
		return 0;
	}
	dev_dbg(&client->dev, "%s, reg %x len %x, val %x\n", __func__, reg, len,
		val);
	buf[0] = reg;
	put_unaligned_be32(val << 8 * (4 - len), buf + 1);

	ret = i2c_master_send(client, buf, len + 1);
	if (ret != len + 1) {
		dev_err(&client->dev, "failed to write reg %d val %d", reg,
			val);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}

static int ov02e10_write_reg_list(struct ov02e10 *ov02e10,
				  const struct ov02e10_reg_list *r_list)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov02e10->sd);
	unsigned int i;
	int ret;

	for (i = 0; i < r_list->num_of_regs; i++) {
		ret = ov02e10_write_reg(ov02e10, r_list->regs[i].address, 1,
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

static int ov02e10_test_pattern(struct ov02e10 *ov02e10, u32 pattern)
{
	if (pattern)
		pattern = pattern << OV02E10_TEST_PATTERN_BAR_SHIFT |
		    OV02E10_TEST_PATTERN_ENABLE;

	return ov02e10_write_reg(ov02e10, (u8) OV02E10_REG_TEST_PATTERN, 1,
				 pattern);
}

static int ov02e10_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov02e10 *ov02e10 = container_of(ctrl->handler,
					       struct ov02e10, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&ov02e10->sd);
	s64 exposure_max;
	int ret;

	/* Propagate change of current control to all related controls */
	if (ctrl->id == V4L2_CID_VBLANK) {
		/* Update max exposure while meeting expected vblanking */
		exposure_max = ov02e10->cur_mode->height + ctrl->val -
		    OV02E10_EXPOSURE_MAX_MARGIN;
		__v4l2_ctrl_modify_range(ov02e10->exposure,
					 ov02e10->exposure->minimum,
					 exposure_max, ov02e10->exposure->step,
					 exposure_max);
	}

	/* V4L2 controls values will be applied only when power is already up */
	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;
	ret = ov02e10_write_reg(ov02e10, OV02E10_REG_COMMAND_UPDATE, 1,
				OV02E10_COMMAND_HOLD);

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&client->dev, "set analog gain\n");
		ret = ov02e10_write_reg(ov02e10, OV02E10_REG_PAGE_FLAG, 1,
					OV02E10_PAGE_1);
		ret = ov02e10_write_reg(ov02e10, OV02E10_REG_ANALOG_GAIN, 1,
					ctrl->val);
		break;

	case V4L2_CID_DIGITAL_GAIN:
		dev_dbg(&client->dev, "set digital gain\n");
		ret = ov02e10_write_reg(ov02e10, OV02E10_REG_PAGE_FLAG, 1,
					OV02E10_PAGE_1);
		ret = ov02e10_write_reg(ov02e10, OV02E10_REG_DIGITAL_GAIN, 2,
					ctrl->val);
		break;

	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "set exposure\n");
		ret = ov02e10_write_reg(ov02e10, OV02E10_REG_PAGE_FLAG, 1,
					OV02E10_PAGE_1);
		ret =
		    ov02e10_write_reg(ov02e10, OV02E10_REG_EXPOSURE, 2,
				      ctrl->val);
		break;

	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set vblank\n");
		ret = ov02e10_write_reg(ov02e10, OV02E10_REG_PAGE_FLAG, 1,
					OV02E10_PAGE_1);
		ret = ov02e10_write_reg(ov02e10, OV02E10_REG_VTS, 2,
					ov02e10->cur_mode->height + ctrl->val);
		break;

	case V4L2_CID_TEST_PATTERN:
		dev_dbg(&client->dev, "set test pattern\n");
		ret = ov02e10_write_reg(ov02e10, OV02E10_REG_PAGE_FLAG, 1,
					OV02E10_PAGE_1);
		ret = ov02e10_test_pattern(ov02e10, ctrl->val);
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
	dev_dbg(&client->dev, "will update cmd\n");
	ret |= ov02e10_write_reg(ov02e10, OV02E10_REG_COMMAND_UPDATE, 1,
				 OV02E10_COMMAND_UPDATE);

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops ov02e10_ctrl_ops = {
	.s_ctrl = ov02e10_set_ctrl,
};

static int ov02e10_init_controls(struct ov02e10 *ov02e10)
{
	struct v4l2_ctrl_handler *ctrl_hdlr;
	const struct ov02e10_mode *cur_mode;
	s64 exposure_max, h_blank, pixel_rate;
	u32 vblank_min, vblank_max, vblank_default;
	int size;
	int ret;

	ctrl_hdlr = &ov02e10->ctrl_handler;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
    IS_ENABLED(CONFIG_INTEL_VSC)
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 9);
#else
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 8);
#endif

	if (ret)
		return ret;

	ctrl_hdlr->lock = &ov02e10->mutex;
	cur_mode = ov02e10->cur_mode;
	size = ARRAY_SIZE(link_freq_menu_items);

	ov02e10->link_freq =
	    v4l2_ctrl_new_int_menu(ctrl_hdlr, &ov02e10_ctrl_ops,
				   V4L2_CID_LINK_FREQ, size - 1, 0,
				   link_freq_menu_items);
	if (ov02e10->link_freq)
		ov02e10->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	pixel_rate = to_pixel_rate(OV02E10_LINK_FREQ_360MHZ_INDEX);
	ov02e10->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &ov02e10_ctrl_ops,
						V4L2_CID_PIXEL_RATE, 0,
						pixel_rate, 1, pixel_rate);

	vblank_min = cur_mode->vts_min - cur_mode->height;
	vblank_max = OV02E10_VTS_MAX - cur_mode->height;
	vblank_default = cur_mode->vts_def - cur_mode->height;
	ov02e10->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &ov02e10_ctrl_ops,
					    V4L2_CID_VBLANK, vblank_min,
					    vblank_max, 1, vblank_default);

	h_blank = to_pixels_per_line(cur_mode->hts, cur_mode->link_freq_index);
	h_blank -= cur_mode->width;
	ov02e10->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &ov02e10_ctrl_ops,
					    V4L2_CID_HBLANK, h_blank, h_blank,
					    1, h_blank);
	if (ov02e10->hblank)
		ov02e10->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
    IS_ENABLED(CONFIG_INTEL_VSC)
	ov02e10->privacy_status = v4l2_ctrl_new_std(ctrl_hdlr, &ov02e10_ctrl_ops,
						    V4L2_CID_PRIVACY, 0, 1, 1,
						    !(ov02e10->status.status));
#endif

	v4l2_ctrl_new_std(ctrl_hdlr, &ov02e10_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  OV02E10_ANAL_GAIN_MIN, OV02E10_ANAL_GAIN_MAX,
			  OV02E10_ANAL_GAIN_STEP, OV02E10_ANAL_GAIN_MIN);
	v4l2_ctrl_new_std(ctrl_hdlr, &ov02e10_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  OV02E10_DGTL_GAIN_MIN, OV02E10_DGTL_GAIN_MAX,
			  OV02E10_DGTL_GAIN_STEP, OV02E10_DGTL_GAIN_DEFAULT);
	exposure_max = cur_mode->vts_def - OV02E10_EXPOSURE_MAX_MARGIN;
	ov02e10->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &ov02e10_ctrl_ops,
					      V4L2_CID_EXPOSURE,
					      OV02E10_EXPOSURE_MIN,
					      exposure_max,
					      OV02E10_EXPOSURE_STEP,
					      exposure_max);
	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &ov02e10_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(ov02e10_test_pattern_menu) - 1,
				     0, 0, ov02e10_test_pattern_menu);
	if (ctrl_hdlr->error)
		return ctrl_hdlr->error;

	ov02e10->sd.ctrl_handler = ctrl_hdlr;

	return 0;
}

static void ov02e10_update_pad_format(const struct ov02e10_mode *mode,
				      struct v4l2_mbus_framefmt *fmt)
{
	fmt->width = mode->width;
	fmt->height = mode->height;
	fmt->code = MEDIA_BUS_FMT_SGRBG10_1X10;
	fmt->field = V4L2_FIELD_NONE;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
    IS_ENABLED(CONFIG_INTEL_VSC)
static void ov02e10_vsc_privacy_callback(void *handle,
				       enum vsc_privacy_status status)
{
	struct ov02e10 *ov02e10 = handle;

	v4l2_ctrl_s_ctrl(ov02e10->privacy_status, !status);
}
#endif

static int ov02e10_start_streaming(struct ov02e10 *ov02e10)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov02e10->sd);
	const struct ov02e10_reg_list *reg_list;
	int ret;

	dev_dbg(&client->dev, "start to set sensor settings\n");
	reg_list = &ov02e10->cur_mode->reg_list;
	ret = ov02e10_write_reg_list(ov02e10, reg_list);
	if (ret) {
		dev_err(&client->dev, "failed to set mode");
		return ret;
	}
	dev_dbg(&client->dev, "start to set ctrl_handler\n");
	ret = __v4l2_ctrl_handler_setup(ov02e10->sd.ctrl_handler);
	if (ret) {
		dev_err(&client->dev, "setup V4L2 ctrl handler fail\n");
		return ret;
	}

	dev_dbg(&client->dev, "start to streaming\n");
	ret = ov02e10_write_reg_list(ov02e10, &ov02e10_streaming_list);
	if (ret) {
		dev_err(&client->dev, "failed to streaming mode");
		return ret;
	}

	return 0;
}

static void ov02e10_stop_streaming(struct ov02e10 *ov02e10)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov02e10->sd);
	int ret;

	ret = ov02e10_write_reg_list(ov02e10, &ov02e10_standby_list);
	if (ret)
		dev_err(&client->dev, "failed to stop streaming: %d", ret);
}

static int ov02e10_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov02e10 *ov02e10 = to_ov02e10(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (ov02e10->streaming == enable)
		return 0;

	mutex_lock(&ov02e10->mutex);
	if (enable) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			mutex_unlock(&ov02e10->mutex);
			return ret;
		}

		ret = ov02e10_start_streaming(ov02e10);
		if (ret) {
			dev_dbg(&client->dev, "start streaming failed\n");
			enable = 0;
			ov02e10_stop_streaming(ov02e10);
			pm_runtime_put(&client->dev);
		}
	} else {
		ov02e10_stop_streaming(ov02e10);
		pm_runtime_put(&client->dev);
	}

	ov02e10->streaming = enable;
	mutex_unlock(&ov02e10->mutex);

	return ret;
}

/* This function tries to get power control resources */
static int ov02e10_get_pm_resources(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ov02e10 *ov02e10 = to_ov02e10(sd);
	int ret;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
    IS_ENABLED(CONFIG_INTEL_VSC)
	acpi_handle handle = ACPI_HANDLE(dev);
	struct acpi_handle_list dep_devices;
	acpi_status status;
	int i = 0;

	ov02e10->use_intel_vsc = false;
	if (!acpi_has_method(handle, "_DEP"))
		return false;

	status = acpi_evaluate_reference(handle, "_DEP", NULL, &dep_devices);
	if (ACPI_FAILURE(status)) {
		acpi_handle_debug(handle, "Failed to evaluate _DEP.\n");
		return false;
	}
	for (i = 0; i < dep_devices.count; i++) {
		struct acpi_device *dep_device = NULL;

		if (dep_devices.handles[i])
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
			acpi_bus_get_device(dep_devices.handles[i], &dep_device);
#else
			dep_device =
				acpi_fetch_acpi_dev(dep_devices.handles[i]);
#endif

		if (dep_device && acpi_match_device_ids(dep_device, cvfd_ids) == 0) {
			ov02e10->use_intel_vsc = true;
			return 0;
		}
	}
#endif
	ov02e10->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ov02e10->reset))
		return dev_err_probe(dev, PTR_ERR(ov02e10->reset),
				     "failed to get reset gpio\n");

	ov02e10->handshake = devm_gpiod_get_optional(dev, "handshake",
						   GPIOD_OUT_LOW);
	if (IS_ERR(ov02e10->handshake))
		return dev_err_probe(dev, PTR_ERR(ov02e10->handshake),
				     "failed to get handshake gpio\n");

	ov02e10->img_clk = devm_clk_get_optional(dev, NULL);
	if (IS_ERR(ov02e10->img_clk))
		return dev_err_probe(dev, PTR_ERR(ov02e10->img_clk),
				     "failed to get imaging clock\n");

	ov02e10->avdd = devm_regulator_get_optional(dev, "avdd");
	if (IS_ERR(ov02e10->avdd)) {
		ret = PTR_ERR(ov02e10->avdd);
		ov02e10->avdd = NULL;
		if (ret != -ENODEV)
			return dev_err_probe(dev, ret,
					     "failed to get avdd regulator\n");
	}

	return 0;
}

static int ov02e10_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ov02e10 *ov02e10 = to_ov02e10(sd);
	int ret = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
    IS_ENABLED(CONFIG_INTEL_VSC)
	if (ov02e10->use_intel_vsc) {
		ret = vsc_release_camera_sensor(&ov02e10->status);
		if (ret && ret != -EAGAIN)
			dev_err(dev, "Release VSC failed");

		return ret;
	}
#endif
	gpiod_set_value_cansleep(ov02e10->reset, 1);
	gpiod_set_value_cansleep(ov02e10->handshake, 0);

	if (ov02e10->avdd)
		ret = regulator_disable(ov02e10->avdd);

	clk_disable_unprepare(ov02e10->img_clk);

	return ret;
}

static int ov02e10_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ov02e10 *ov02e10 = to_ov02e10(sd);
	int ret;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && \
    IS_ENABLED(CONFIG_INTEL_VSC)
	if (ov02e10->use_intel_vsc) {
		ov02e10->conf.lane_num = OV02E10_DATA_LANES;
		/* frequency unit 100k */
		ov02e10->conf.freq = OV02E10_LINK_FREQ_360MHZ / 100000;
		ret = vsc_acquire_camera_sensor(&ov02e10->conf,
						ov02e10_vsc_privacy_callback,
						ov02e10, &ov02e10->status);
		if (ret == -EAGAIN)
			return -EPROBE_DEFER;
		if (ret) {
			dev_err(dev, "Acquire VSC failed");
			return ret;
		}
		if (ov02e10->privacy_status)
			__v4l2_ctrl_s_ctrl(ov02e10->privacy_status,
					!(ov02e10->status.status));

		return ret;
	}
#endif
	ret = clk_prepare_enable(ov02e10->img_clk);
	if (ret < 0) {
		dev_err(dev, "failed to enable imaging clock: %d", ret);
		return ret;
	}

	if (ov02e10->avdd) {
		ret = regulator_enable(ov02e10->avdd);
		if (ret < 0) {
			dev_err(dev, "failed to enable avdd: %d", ret);
			clk_disable_unprepare(ov02e10->img_clk);
			return ret;
		}
	}
	gpiod_set_value_cansleep(ov02e10->handshake, 1);
	gpiod_set_value_cansleep(ov02e10->reset, 0);

	/* Lattice MIPI aggregator with some version FW needs longer delay
	   after handshake triggered. We set 25ms as a safe value and wait
	   for a stable version FW. */
	msleep_interruptible(25);

	return ret;
}

static int __maybe_unused ov02e10_suspend(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ov02e10 *ov02e10 = to_ov02e10(sd);

	mutex_lock(&ov02e10->mutex);
	if (ov02e10->streaming)
		ov02e10_stop_streaming(ov02e10);

	mutex_unlock(&ov02e10->mutex);

	return 0;
}

static int __maybe_unused ov02e10_resume(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ov02e10 *ov02e10 = to_ov02e10(sd);
	int ret = 0;

	mutex_lock(&ov02e10->mutex);
	if (!ov02e10->streaming)
		goto exit;

	ret = ov02e10_start_streaming(ov02e10);
	if (ret) {
		ov02e10->streaming = false;
		ov02e10_stop_streaming(ov02e10);
	}

exit:
	mutex_unlock(&ov02e10->mutex);
	return ret;
}

static int ov02e10_set_format(struct v4l2_subdev *sd,
			      struct v4l2_subdev_state *sd_state,
			      struct v4l2_subdev_format *fmt)
{
	struct ov02e10 *ov02e10 = to_ov02e10(sd);
	const struct ov02e10_mode *mode;
	s32 vblank_def, h_blank;

	mode = v4l2_find_nearest_size(supported_modes,
				      ARRAY_SIZE(supported_modes),
				      width, height, fmt->format.width,
				      fmt->format.height);

	mutex_lock(&ov02e10->mutex);
	ov02e10_update_pad_format(mode, &fmt->format);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) =
		    fmt->format;
#else
		*v4l2_subdev_state_get_format(sd_state, fmt->pad) =
		    fmt->format;
#endif
	} else {
		ov02e10->cur_mode = mode;
		__v4l2_ctrl_s_ctrl(ov02e10->link_freq, mode->link_freq_index);
		__v4l2_ctrl_s_ctrl_int64(ov02e10->pixel_rate,
					 to_pixel_rate(mode->link_freq_index));

		/* Update limits and set FPS to default */
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(ov02e10->vblank,
					 mode->vts_min - mode->height,
					 OV02E10_VTS_MAX - mode->height, 1,
					 vblank_def);
		__v4l2_ctrl_s_ctrl(ov02e10->vblank, vblank_def);
		h_blank = to_pixels_per_line(mode->hts, mode->link_freq_index) -
		    mode->width;
		__v4l2_ctrl_modify_range(ov02e10->hblank, h_blank, h_blank, 1,
					 h_blank);
	}
	mutex_unlock(&ov02e10->mutex);

	return 0;
}

static int ov02e10_get_format(struct v4l2_subdev *sd,
			      struct v4l2_subdev_state *sd_state,
			      struct v4l2_subdev_format *fmt)
{
	struct ov02e10 *ov02e10 = to_ov02e10(sd);

	mutex_lock(&ov02e10->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
		fmt->format = *v4l2_subdev_get_try_format(&ov02e10->sd,
							  sd_state, fmt->pad);
#else
		fmt->format = *v4l2_subdev_state_get_format(
							  sd_state, fmt->pad);
#endif
	else
		ov02e10_update_pad_format(ov02e10->cur_mode, &fmt->format);

	mutex_unlock(&ov02e10->mutex);

	return 0;
}

static int ov02e10_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index > 0)
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_SGRBG10_1X10;

	return 0;
}

static int ov02e10_enum_frame_size(struct v4l2_subdev *sd,
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

static int ov02e10_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ov02e10 *ov02e10 = to_ov02e10(sd);

	mutex_lock(&ov02e10->mutex);
	ov02e10_update_pad_format(&supported_modes[0],
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
				  v4l2_subdev_get_try_format(sd, fh->state, 0));
#else
				  v4l2_subdev_state_get_format(fh->state, 0));
#endif
	mutex_unlock(&ov02e10->mutex);

	return 0;
}

static const struct v4l2_subdev_video_ops ov02e10_video_ops = {
	.s_stream = ov02e10_set_stream,
};

static const struct v4l2_subdev_pad_ops ov02e10_pad_ops = {
	.set_fmt = ov02e10_set_format,
	.get_fmt = ov02e10_get_format,
	.enum_mbus_code = ov02e10_enum_mbus_code,
	.enum_frame_size = ov02e10_enum_frame_size,
};

static const struct v4l2_subdev_ops ov02e10_subdev_ops = {
	.video = &ov02e10_video_ops,
	.pad = &ov02e10_pad_ops,
};

static const struct media_entity_operations ov02e10_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_subdev_internal_ops ov02e10_internal_ops = {
	.open = ov02e10_open,
};

static int ov02e10_identify_module(struct ov02e10 *ov02e10)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov02e10->sd);
	int ret;
	u32 val;

	ret = ov02e10_write_reg(ov02e10, OV02E10_REG_PAGE_FLAG, 1,
				OV02E10_PAGE_0);
	if (ret)
		return ret;

	ret = ov02e10_read_reg(ov02e10, OV02E10_REG_CHIP_ID, 4, &val);
	if (ret)
		return ret;

	if (val != OV02E10_CHIP_ID) {
		dev_err(&client->dev, "chip id mismatch: %x!=%x",
			OV02E10_CHIP_ID, val);
		return -ENXIO;
	}

	return 0;
}

static int ov02e10_check_hwcfg(struct device *dev)
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

	if (bus_cfg.bus.mipi_csi2.num_data_lanes != OV02E10_DATA_LANES) {
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
static int ov02e10_remove(struct i2c_client *client)
#else
static void ov02e10_remove(struct i2c_client *client)
#endif
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov02e10 *ov02e10 = to_ov02e10(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);
	pm_runtime_disable(&client->dev);
	mutex_destroy(&ov02e10->mutex);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

static int ov02e10_probe(struct i2c_client *client)
{
	struct ov02e10 *ov02e;
	int ret;

	/* Check HW config */
	ret = ov02e10_check_hwcfg(&client->dev);
	if (ret) {
		dev_err(&client->dev, "failed to check hwcfg: %d", ret);
		return ret;
	}

	ov02e = devm_kzalloc(&client->dev, sizeof(*ov02e), GFP_KERNEL);
	if (!ov02e)
		return -ENOMEM;

	/* Initialize subdev */
	v4l2_i2c_subdev_init(&ov02e->sd, client, &ov02e10_subdev_ops);
	ov02e10_get_pm_resources(&client->dev);

	ret = ov02e10_power_on(&client->dev);
	if (ret) {
		dev_err_probe(&client->dev, ret, "failed to power on\n");
		goto error_power_off;
	}

	/* Check module identity */
	ret = ov02e10_identify_module(ov02e);
	if (ret) {
		dev_err(&client->dev, "failed to find sensor: %d\n", ret);
		goto error_power_off;
	}

	/* Set default mode to max resolution */
	ov02e->cur_mode = &supported_modes[0];

	dev_dbg(&client->dev, "will Init controls\n");
	ret = ov02e10_init_controls(ov02e);
	if (ret)
		return ret;

	/* Initialize subdev */
	ov02e->sd.internal_ops = &ov02e10_internal_ops;
	ov02e->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ov02e->sd.entity.ops = &ov02e10_subdev_entity_ops;
	ov02e->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pad */
	ov02e->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&ov02e->sd.entity, 1, &ov02e->pad);
	if (ret) {
		dev_err(&client->dev, "%s failed:%d\n", __func__, ret);
		goto error_handler_free;
	}

	ret = v4l2_async_register_subdev_sensor(&ov02e->sd);
	if (ret < 0) {
		dev_err(&client->dev, "async reg subdev error\n");
		goto error_media_entity;
	}

	/*
	 * Device is already turned on by i2c-core with ACPI domain PM.
	 * Enable runtime PM and turn off the device.
	 */
	pm_runtime_set_active(&client->dev);
	pm_runtime_enable(&client->dev);
	pm_runtime_idle(&client->dev);

	return 0;

error_media_entity:
	media_entity_cleanup(&ov02e->sd.entity);

error_handler_free:
	v4l2_ctrl_handler_free(ov02e->sd.ctrl_handler);
	mutex_destroy(&ov02e->mutex);
	dev_err(&client->dev, "%s failed:%d\n", __func__, ret);
error_power_off:
	ov02e10_power_off(&client->dev);

	dev_dbg(&client->dev, "probe done\n");
	return ret;
}

static const struct dev_pm_ops ov02e10_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ov02e10_suspend, ov02e10_resume)
	    SET_RUNTIME_PM_OPS(ov02e10_power_off, ov02e10_power_on, NULL)
};

static const struct acpi_device_id ov02e10_acpi_ids[] = {
	{ "OVTI02E1" },
	{ }
};

MODULE_DEVICE_TABLE(acpi, ov02e10_acpi_ids);

static struct i2c_driver ov02e10_i2c_driver = {
	.driver = {
		   .name = "ov02e10",
		   .pm = &ov02e10_pm_ops,
		   .acpi_match_table = ov02e10_acpi_ids,
		    },
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
	.probe_new = ov02e10_probe,
#else
	.probe = ov02e10_probe,
#endif
	.remove = ov02e10_remove,
};

module_i2c_driver(ov02e10_i2c_driver);

MODULE_AUTHOR("Jingjing Xiong <jingjing.xiong@intel.com>");
MODULE_DESCRIPTION("OmniVision OV02E10 sensor driver");
MODULE_LICENSE("GPL v2");
