// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2024 Intel Corporation.

#include <asm/unaligned.h>
#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/units.h>
#include <linux/regmap.h>
#include <media/v4l2-cci.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>

#define OV05C10_REG_CHIP_ID_H	CCI_REG16(0x00)	//P0:0x00[7:0],0x01[7:0]
#define OV05C10_REG_CHIP_ID_L	CCI_REG16(0x02)	//P0:0x02[7:0],0x03[7:0]
#define OV05C10_CHIP_ID		0x43055610

#define REG_TIMING_HTS		CCI_REG16(0x37) //P1:0x37[4:0],0x38[7:0]
#define REG_TIMING_VTS		CCI_REG16(0x35) //P1:0x35[7:0],0x36[7:0]
#define REG_DUMMY_LINE		CCI_REG16(0x05) //P1:0x05[7:0],0x06[7:0]
#define REG_EXPOSURE		CCI_REG24(0x02) //P1:0x02[7:0]--0x04[7:0]
#define REG_ANALOG_GAIN		CCI_REG8(0x24)
#define REG_DIGITAL_GAIN	CCI_REG16(0x21)	//P1:0x21[2:0],0x22[7:0]

#define REG_PAGE_FLAG		CCI_REG8(0xfd)
#define PAGE_0			0x0
#define PAGE_1			0x1

#define OV05C10_EXPOSURE_MARGIN		33
#define OV05C10_EXPOSURE_MIN		0x6
#define OV05C10_EXPOSURE_STEP		0x1

#define MAX_ANA_GAIN			0xf8  // 15.5x
#define MIN_ANA_GAIN			0x10  // 1x
#define OV05C10_ANAL_GAIN_STEP		0x01
#define OV05C10_ANAL_GAIN_DEFAULT	0x10

#define MAX_DIG_GAIN			0x100 // 4x
#define MIN_DIG_GAIN			0x40  // 1x
#define OV05C10_DGTL_GAIN_STEP		0x01
#define OV05C10_DGTL_GAIN_DEFAULT	0x40

#define OV05C10_VTS_MAX			0xffff
#define OV05C10_PPL			3236
#define OV05C10_DEFAULT_VTS		1860
#define PIXEL_RATE			192000000ULL
#define OV05C10_NATIVE_WIDTH		2888
#define OV05C10_NATIVE_HEIGHT		1808

#define to_ov05c10(_sd)		container_of(_sd, struct ov05c10, sd)

static const char *const ov05c10_test_pattern_menu[] = {
	"No Pattern",
	"Color Bar",
};
static const s64 ov05c10_link_freq_menu_items[] = {
	480000000ULL,
};

struct ov05c10_reg_list {
	u32 num_of_regs;
	const struct cci_reg_sequence *regs;
};

struct ov05c10_mode {
	u32 width;
	u32 height;
	u32 hts;
	u32 vts_def;
	u32 vts_min;
	u32 link_freq_index;
	u32 code;
	u32 fps;
	s32 lanes;
	/* Sensor register settings for this mode */
	const struct ov05c10_reg_list reg_list;
};

static const struct cci_reg_sequence ov05c10_soft_standby[] = {
	{ REG_PAGE_FLAG, PAGE_0 },
	{ CCI_REG8(0xa0), 0x00 },
	{ REG_PAGE_FLAG, PAGE_1 },
	{ CCI_REG8(0x01), 0x02 },
};

static const struct cci_reg_sequence ov05c10_streaming[] = {
	{ REG_PAGE_FLAG, PAGE_0 },
	{ CCI_REG8(0xa0), 0x01 },
	{ REG_PAGE_FLAG, PAGE_1 },
	{ CCI_REG8(0x01), 0x02 },
};

static const struct cci_reg_sequence ov05c10_test_enable[] = {
	{ CCI_REG8(0xf3), 0x02 },
	{ CCI_REG8(0x12), 0x01 },
};

static const struct cci_reg_sequence ov05c10_test_disable[] = {
	{ CCI_REG8(0xf3), 0x00 },
	{ CCI_REG8(0x12), 0x00 },
};

static const struct cci_reg_sequence ov05c10_trigger[] = {
	{ REG_PAGE_FLAG, PAGE_1 },
	{ CCI_REG8(0x01), 0x01 },
};

//2800X1576_2lane_raw10_Mclk19.2M_pclk96M_30fps
static const struct cci_reg_sequence mode_2800_1576_30fps[] = {
	{ CCI_REG8(0xfd), 0x00 },
	{ CCI_REG8(0x20), 0x00 },
	{ CCI_REG8(0xfd), 0x00 },
	{ CCI_REG8(0x20), 0x0b },
	{ CCI_REG8(0xc1), 0x09 },
	{ CCI_REG8(0x21), 0x06 },
	{ CCI_REG8(0x11), 0x4e },
	{ CCI_REG8(0x12), 0x13 },
	{ CCI_REG8(0x14), 0x96 },
	{ CCI_REG8(0x1b), 0x64 },
	{ CCI_REG8(0x1d), 0x02 },
	{ CCI_REG8(0x1e), 0x40 },
	{ CCI_REG8(0xe7), 0x03 },
	{ CCI_REG8(0xe7), 0x00 },
	{ CCI_REG8(0x21), 0x00 },
	{ CCI_REG8(0xfd), 0x01 },
	{ CCI_REG8(0x03), 0x00 },
	{ CCI_REG8(0x04), 0x06 },
	{ CCI_REG8(0x06), 0x76 },
	{ CCI_REG8(0x07), 0x08 },
	{ CCI_REG8(0x1b), 0x01 },
	{ CCI_REG8(0x24), 0xff },
	{ CCI_REG8(0x42), 0x5d },
	{ CCI_REG8(0x43), 0x08 },
	{ CCI_REG8(0x44), 0x81 },
	{ CCI_REG8(0x46), 0x5f },
	{ CCI_REG8(0x48), 0x18 },
	{ CCI_REG8(0x49), 0x04 },
	{ CCI_REG8(0x5c), 0x18 },
	{ CCI_REG8(0x5e), 0x13 },
	{ CCI_REG8(0x70), 0x15 },
	{ CCI_REG8(0x77), 0x35 },
	{ CCI_REG8(0x79), 0xb2 },
	{ CCI_REG8(0x7b), 0x08 },
	{ CCI_REG8(0x7d), 0x08 },
	{ CCI_REG8(0x7e), 0x08 },
	{ CCI_REG8(0x7f), 0x08 },
	{ CCI_REG8(0x90), 0x37 },
	{ CCI_REG8(0x91), 0x05 },
	{ CCI_REG8(0x92), 0x18 },
	{ CCI_REG8(0x93), 0x27 },
	{ CCI_REG8(0x94), 0x05 },
	{ CCI_REG8(0x95), 0x38 },
	{ CCI_REG8(0x9b), 0x00 },
	{ CCI_REG8(0x9c), 0x06 },
	{ CCI_REG8(0x9d), 0x28 },
	{ CCI_REG8(0x9e), 0x06 },
	{ CCI_REG8(0xb2), 0x0f },
	{ CCI_REG8(0xb3), 0x29 },
	{ CCI_REG8(0xbf), 0x3c },
	{ CCI_REG8(0xc2), 0x04 },
	{ CCI_REG8(0xc4), 0x00 },
	{ CCI_REG8(0xca), 0x20 },
	{ CCI_REG8(0xcb), 0x20 },
	{ CCI_REG8(0xcc), 0x28 },
	{ CCI_REG8(0xcd), 0x28 },
	{ CCI_REG8(0xce), 0x20 },
	{ CCI_REG8(0xcf), 0x20 },
	{ CCI_REG8(0xd0), 0x2a },
	{ CCI_REG8(0xd1), 0x2a },
	{ CCI_REG8(0xfd), 0x0f },
	{ CCI_REG8(0x00), 0x00 },
	{ CCI_REG8(0x01), 0xa0 },
	{ CCI_REG8(0x02), 0x48 },
	{ CCI_REG8(0x07), 0x8e },
	{ CCI_REG8(0x08), 0x70 },
	{ CCI_REG8(0x09), 0x01 },
	{ CCI_REG8(0x0b), 0x40 },
	{ CCI_REG8(0x0d), 0x07 },
	{ CCI_REG8(0x11), 0x33 },
	{ CCI_REG8(0x12), 0x77 },
	{ CCI_REG8(0x13), 0x66 },
	{ CCI_REG8(0x14), 0x65 },
	{ CCI_REG8(0x15), 0x37 },
	{ CCI_REG8(0x16), 0xbf },
	{ CCI_REG8(0x17), 0xff },
	{ CCI_REG8(0x18), 0xff },
	{ CCI_REG8(0x19), 0x12 },
	{ CCI_REG8(0x1a), 0x10 },
	{ CCI_REG8(0x1c), 0x77 },
	{ CCI_REG8(0x1d), 0x77 },
	{ CCI_REG8(0x20), 0x0f },
	{ CCI_REG8(0x21), 0x0f },
	{ CCI_REG8(0x22), 0x0f },
	{ CCI_REG8(0x23), 0x0f },
	{ CCI_REG8(0x2b), 0x20 },
	{ CCI_REG8(0x2c), 0x20 },
	{ CCI_REG8(0x2d), 0x04 },
	{ CCI_REG8(0xfd), 0x03 },
	{ CCI_REG8(0x9d), 0x0f },
	{ CCI_REG8(0x9f), 0x40 },
	{ CCI_REG8(0xfd), 0x00 },
	{ CCI_REG8(0x20), 0x1b },
	{ CCI_REG8(0xfd), 0x04 },
	{ CCI_REG8(0x19), 0x60 },
	{ CCI_REG8(0xfd), 0x02 },
	{ CCI_REG8(0x75), 0x04 },
	{ CCI_REG8(0x7f), 0x06 },
	{ CCI_REG8(0x9a), 0x03 },
	{ CCI_REG8(0xa0), 0x00 },
	{ CCI_REG8(0xa1), 0x75 }, //;GRBG
	{ CCI_REG8(0xa2), 0x06 }, //;vsize[11:8]
	{ CCI_REG8(0xa3), 0x28 }, //;vsize[7:0]
	{ CCI_REG8(0xa4), 0x00 },
	{ CCI_REG8(0xa5), 0x2e },
	{ CCI_REG8(0xa6), 0x0a }, //;hsize[11:8]
	{ CCI_REG8(0xa7), 0xf0 }, //;hsize[7:0]
	{ CCI_REG8(0xfd), 0x00 }, //;mipi size
	{ CCI_REG8(0x8e), 0x0a }, //;hsize[11:8]
	{ CCI_REG8(0x8f), 0xf0 }, //;hsize[7:0]
	{ CCI_REG8(0x90), 0x06 }, //;vsize[11:8]
	{ CCI_REG8(0x91), 0x28 }, //;vsize[7:0]
	{ CCI_REG8(0xfd), 0x07 },
	{ CCI_REG8(0x42), 0x00 },
	{ CCI_REG8(0x43), 0x80 },
	{ CCI_REG8(0x44), 0x00 },
	{ CCI_REG8(0x45), 0x80 },
	{ CCI_REG8(0x46), 0x00 },
	{ CCI_REG8(0x47), 0x80 },
	{ CCI_REG8(0x48), 0x00 },
	{ CCI_REG8(0x49), 0x80 },
	{ CCI_REG8(0x00), 0xf7 },
	{ CCI_REG8(0xfd), 0x00 },
	{ CCI_REG8(0xe7), 0x03 },
	{ CCI_REG8(0xe7), 0x00 },
	{ CCI_REG8(0xfd), 0x00 },
	{ CCI_REG8(0x93), 0x18 },
	{ CCI_REG8(0x94), 0xff },
	{ CCI_REG8(0x95), 0xbd },
	{ CCI_REG8(0x96), 0x1a },
	{ CCI_REG8(0x98), 0x04 },
	{ CCI_REG8(0x99), 0x08 },
	{ CCI_REG8(0x9b), 0x10 },
	{ CCI_REG8(0x9c), 0x3f },
	{ CCI_REG8(0xa1), 0x05 },
	{ CCI_REG8(0xa4), 0x2f },
	{ CCI_REG8(0xc0), 0x0c },
	{ CCI_REG8(0xc1), 0x08 },
	{ CCI_REG8(0xc2), 0x00 },
	{ CCI_REG8(0xb6), 0x20 },
	{ CCI_REG8(0xbb), 0x80 },
	{ CCI_REG8(0xfd), 0x00 },
	{ CCI_REG8(0xa0), 0x00 }, //disable MIPI
	{ CCI_REG8(0xfd), 0x01 },
	{ CCI_REG8(0x33), 0x03 },
	{ CCI_REG8(0x01), 0x02 },
	{ CCI_REG8(0xfd), 0x00 },
	{ CCI_REG8(0x20), 0x1f },
	{ CCI_REG8(0xfd), 0x01 },
};

static const struct ov05c10_mode supported_modes[] = {
	{
		.width = 2800,
		.height = 1576,
		.hts = 758,
		.vts_def = 1978,
		.vts_min = 1978,
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.fps = 30,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_2800_1576_30fps),
			.regs = mode_2800_1576_30fps,
		},
		.link_freq_index = 0,
	},
};

struct ov05c10 {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;

	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *analogue_gain;
	struct v4l2_ctrl *digital_gain;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;

	struct regmap *regmap;
	unsigned long link_freq_bitmap;
	const struct ov05c10_mode *cur_mode;

	struct clk *img_clk;
	struct regulator *avdd;
	struct gpio_desc *reset;
};

static int ov05c10_test_pattern(struct ov05c10 *ov05c10, u32 pattern)
{
	int ret;

	ret = cci_write(ov05c10->regmap, REG_PAGE_FLAG, 0x04, NULL);
	if (ret)
		return ret;
	if (pattern)
		return cci_multi_reg_write(ov05c10->regmap,
					   ov05c10_test_enable,
					   ARRAY_SIZE(ov05c10_test_enable),
					   NULL);
	else
		return cci_multi_reg_write(ov05c10->regmap,
					   ov05c10_test_disable,
					   ARRAY_SIZE(ov05c10_test_disable),
					   NULL);
}

static int ov05c10_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov05c10 *ov05c10 =
		container_of(ctrl->handler, struct ov05c10, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&ov05c10->sd);
	struct v4l2_subdev_state *state;
	const struct v4l2_mbus_framefmt *format;
	s64 exposure_max;
	u64 vts;
	int ret;

	state = v4l2_subdev_get_locked_active_state(&ov05c10->sd);
	format = v4l2_subdev_state_get_format(state, 0);

	/* Propagate change of current control to all related controls */
	if (ctrl->id == V4L2_CID_VBLANK) {
		/* Update max exposure while meeting expected vblanking */
		exposure_max =
			format->height + ctrl->val - OV05C10_EXPOSURE_MARGIN;
		__v4l2_ctrl_modify_range(ov05c10->exposure,
			ov05c10->exposure->minimum, exposure_max,
			ov05c10->exposure->step,
			ov05c10->cur_mode->height - OV05C10_EXPOSURE_MARGIN);

		/*
		 * REG_TIMING_VTS is read-only and increased by writing to
		 * REG_DUMMY_LINE in ov05c10. The calculation formula is
		 * required VTS = dummyline + current VTS.
		 * Here get the current VTS and calculate the required dummyline.
		 */
		cci_read(ov05c10->regmap, REG_TIMING_VTS, &vts, NULL);
		ctrl->val += format->height;
		ctrl->val = (ctrl->val > vts) ? ctrl->val - vts : 0;
	}
	/* V4L2 controls values will be applied only when power is already up */
	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	ret = cci_write(ov05c10->regmap, REG_PAGE_FLAG, PAGE_1, NULL);
	if (ret) {
		dev_err(&client->dev, "failed to set ctrl");
		goto err;
	}

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		ret = cci_write(ov05c10->regmap, REG_ANALOG_GAIN,
				ctrl->val, NULL);
		break;
	case V4L2_CID_DIGITAL_GAIN:
		ret = cci_write(ov05c10->regmap, REG_DIGITAL_GAIN,
				ctrl->val, NULL);
		break;
	case V4L2_CID_EXPOSURE:
		ret = cci_write(ov05c10->regmap, REG_EXPOSURE,
				ctrl->val, NULL);
		break;
	case V4L2_CID_VBLANK:
		ret = cci_write(ov05c10->regmap, REG_DUMMY_LINE,
				ctrl->val, NULL);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = ov05c10_test_pattern(ov05c10, ctrl->val);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	ret = cci_multi_reg_write(ov05c10->regmap, ov05c10_trigger,
				  ARRAY_SIZE(ov05c10_trigger), NULL);
	if (ret) {
		dev_err(&client->dev, "failed to trigger write");
		goto err;
	}

err:
	pm_runtime_put(&client->dev);
	return ret;
}

static const struct v4l2_ctrl_ops ov05c10_ctrl_ops = {
	.s_ctrl = ov05c10_set_ctrl,
};

static int ov05c10_init_controls(struct ov05c10 *ov05c10)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov05c10->sd);
	struct v4l2_fwnode_device_properties props;
	struct v4l2_ctrl_handler *ctrl_hdlr;
	s64 exposure_max, vblank_max, vblank_min, vblank_def, hblank;
	int ret;

	ctrl_hdlr = &ov05c10->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 8);
	if (ret)
		return ret;

	ov05c10->link_freq = v4l2_ctrl_new_int_menu(ctrl_hdlr,
		&ov05c10_ctrl_ops, V4L2_CID_LINK_FREQ,
		ARRAY_SIZE(ov05c10_link_freq_menu_items) - 1, 0,
		ov05c10_link_freq_menu_items);
	if (ov05c10->link_freq)
		ov05c10->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_min = ov05c10->cur_mode->vts_min - ov05c10->cur_mode->height;
	vblank_max = OV05C10_VTS_MAX - ov05c10->cur_mode->height;
	vblank_def = ov05c10->cur_mode->vts_def - ov05c10->cur_mode->height;
	ov05c10->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &ov05c10_ctrl_ops,
					    V4L2_CID_VBLANK, vblank_min,
					    vblank_max, 1, vblank_def);

	hblank = OV05C10_PPL - ov05c10->cur_mode->width;
	ov05c10->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &ov05c10_ctrl_ops,
					    V4L2_CID_HBLANK, hblank, hblank, 1,
					    hblank);
	if (ov05c10->hblank)
		ov05c10->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	v4l2_ctrl_new_std(ctrl_hdlr, &ov05c10_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  MIN_ANA_GAIN, MAX_ANA_GAIN, OV05C10_ANAL_GAIN_STEP,
			  OV05C10_ANAL_GAIN_DEFAULT);
	v4l2_ctrl_new_std(ctrl_hdlr, &ov05c10_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  MIN_DIG_GAIN, MAX_DIG_GAIN, OV05C10_DGTL_GAIN_STEP,
			  OV05C10_DGTL_GAIN_DEFAULT);

	exposure_max = ov05c10->cur_mode->vts_def - OV05C10_EXPOSURE_MARGIN;
	ov05c10->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &ov05c10_ctrl_ops,
					      V4L2_CID_EXPOSURE,
					      OV05C10_EXPOSURE_MIN,
					      exposure_max,
					      OV05C10_EXPOSURE_STEP,
					      exposure_max);

	ov05c10->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &ov05c10_ctrl_ops,
						V4L2_CID_PIXEL_RATE,
						PIXEL_RATE,
						PIXEL_RATE, 1,
						PIXEL_RATE);

	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &ov05c10_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(ov05c10_test_pattern_menu) - 1,
				     0, 0, ov05c10_test_pattern_menu);

	if (ctrl_hdlr->error)
		return ctrl_hdlr->error;

	ret = v4l2_fwnode_device_parse(&client->dev, &props);
	if (ret)
		return ret;

	ret = v4l2_ctrl_new_fwnode_properties(ctrl_hdlr, &ov05c10_ctrl_ops,
					      &props);
	if (ret)
		return ret;

	ov05c10->sd.ctrl_handler = ctrl_hdlr;
	return 0;
}

static int ov05c10_start_streaming(struct ov05c10 *ov05c10)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov05c10->sd);
	const struct ov05c10_reg_list *reg_list;
	int ret;

	ret = pm_runtime_resume_and_get(&client->dev);
	if (ret < 0)
		return ret;

	reg_list = &ov05c10->cur_mode->reg_list;
	ret = cci_multi_reg_write(ov05c10->regmap, reg_list->regs,
				  reg_list->num_of_regs, NULL);
	if (ret) {
		dev_err(&client->dev, "failed to set mode");
		goto err_rpm_put;
	}

	ret = __v4l2_ctrl_handler_setup(ov05c10->sd.ctrl_handler);
	if (ret)
		goto err_rpm_put;

	ret = cci_multi_reg_write(ov05c10->regmap, ov05c10_streaming,
				  ARRAY_SIZE(ov05c10_streaming), NULL);
	if (ret) {
		dev_err(&client->dev, "failed to start stream");
		goto err_rpm_put;
	}

	return 0;

err_rpm_put:
	pm_runtime_put(&client->dev);
	return ret;
}

static int ov05c10_stop_streaming(struct ov05c10 *ov05c10)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov05c10->sd);
	int ret;

	ret = cci_multi_reg_write(ov05c10->regmap, ov05c10_soft_standby,
				ARRAY_SIZE(ov05c10_soft_standby), NULL);
	if (ret < 0)
		dev_err(&client->dev, "failed to stop stream");

	pm_runtime_put(&client->dev);
	return ret;
}

static int ov05c10_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov05c10 *ov05c10 = to_ov05c10(sd);
	struct v4l2_subdev_state *state;
	int ret;

	state = v4l2_subdev_lock_and_get_active_state(sd);
	if (enable)
		ret = ov05c10_start_streaming(ov05c10);
	else
		ret = ov05c10_stop_streaming(ov05c10);

	v4l2_subdev_unlock_state(state);
	return ret;
}

static void ov05c10_update_pad_format(const struct ov05c10_mode *mode,
				struct v4l2_mbus_framefmt *fmt)
{
	fmt->width = mode->width;
	fmt->height = mode->height;
	fmt->code = mode->code;
	fmt->field = V4L2_FIELD_NONE;
}

static int ov05c10_set_format(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_format *fmt)
{
	struct ov05c10 *ov05c10 = to_ov05c10(sd);
	struct i2c_client *client = v4l2_get_subdevdata(&ov05c10->sd);
	const struct ov05c10_mode *mode;
	s64 hblank, exposure_max;
	int ret;

	mode = v4l2_find_nearest_size(supported_modes,
				ARRAY_SIZE(supported_modes),
				width, height,
				fmt->format.width,
				fmt->format.height);

	ov05c10_update_pad_format(mode, &fmt->format);
	*v4l2_subdev_state_get_format(sd_state, fmt->pad) = fmt->format;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		return 0;

	ov05c10->cur_mode = mode;
	ret = __v4l2_ctrl_s_ctrl(ov05c10->link_freq, mode->link_freq_index);
	if (ret) {
		dev_err(&client->dev, "Link Freq ctrl set failed\n");
		return ret;
	}

	hblank = OV05C10_PPL - ov05c10->cur_mode->width;
	ret = __v4l2_ctrl_modify_range(ov05c10->hblank, hblank, hblank,
				       1, hblank);
	if (ret) {
		dev_err(&client->dev, "HB ctrl range update failed\n");
		return ret;
	}

	/* Update limits and set FPS to default */
	ret = __v4l2_ctrl_modify_range(ov05c10->vblank,
				       mode->vts_min - mode->height,
				       OV05C10_VTS_MAX - mode->height, 1,
				       mode->vts_def - mode->height);
	if (ret) {
		dev_err(&client->dev, "VB ctrl range update failed\n");
		return ret;
	}

	ret = __v4l2_ctrl_s_ctrl(ov05c10->vblank, mode->vts_def - mode->height);
	if (ret) {
		dev_err(&client->dev, "VB ctrl set failed\n");
		return ret;
	}

	exposure_max = mode->vts_def - OV05C10_EXPOSURE_MARGIN;
	ret = __v4l2_ctrl_modify_range(ov05c10->exposure, OV05C10_EXPOSURE_MIN,
				       exposure_max, OV05C10_EXPOSURE_STEP,
				       exposure_max);
	if (ret) {
		dev_err(&client->dev, "exposure ctrl range update failed\n");
		return ret;
	}

	return 0;
}

static int ov05c10_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index > 0)
		return -EINVAL;
	code->code = MEDIA_BUS_FMT_SGRBG10_1X10;
	return 0;
}

static int ov05c10_enum_frame_size(struct v4l2_subdev *sd,
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

static int ov05c10_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_selection *sel)
{
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP:
		sel->r = *v4l2_subdev_state_get_crop(state, 0);
		break;
	case V4L2_SEL_TGT_CROP_BOUNDS:
		sel->r.top = 0;
		sel->r.left = 0;
		sel->r.width = OV05C10_NATIVE_WIDTH;
		sel->r.height = OV05C10_NATIVE_HEIGHT;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int ov05c10_init_state(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state)
{
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
		.pad = 0,
		.format = {
			.code = MEDIA_BUS_FMT_SGRBG10_1X10,
			.width = 2800,
			.height = 1576,
		},
	};

	ov05c10_set_format(sd, sd_state, &fmt);
	return 0;
}

static const struct v4l2_subdev_video_ops ov05c10_video_ops = {
	.s_stream = ov05c10_s_stream,
};
static const struct v4l2_subdev_pad_ops ov05c10_pad_ops = {
	.set_fmt = ov05c10_set_format,
	.get_fmt = v4l2_subdev_get_fmt,
	.enum_mbus_code = ov05c10_enum_mbus_code,
	.enum_frame_size = ov05c10_enum_frame_size,
	.get_selection = ov05c10_get_selection,
};
static const struct v4l2_subdev_core_ops ov05c10_core_ops = {
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};
static const struct v4l2_subdev_ops ov05c10_subdev_ops = {
	.core = &ov05c10_core_ops,
	.video = &ov05c10_video_ops,
	.pad = &ov05c10_pad_ops,
};
static const struct media_entity_operations ov05c10_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};
static const struct v4l2_subdev_internal_ops ov05c10_internal_ops = {
	.init_state = ov05c10_init_state,
};

static int ov05c10_parse_fwnode(struct ov05c10 *ov05c10, struct device *dev)
{
	struct fwnode_handle *endpoint;
	struct fwnode_handle *fwnode = dev_fwnode(dev);
	struct v4l2_fwnode_endpoint bus_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY,
	};
	int ret;
	unsigned int i, j;

	if (!fwnode)
		return -ENXIO;

	endpoint = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (!endpoint) {
		dev_err(dev, "endpoint node not found\n");
		return -EPROBE_DEFER;
	}

	ret = v4l2_fwnode_endpoint_alloc_parse(endpoint, &bus_cfg);
	fwnode_handle_put(endpoint);
	if (ret) {
		dev_err(dev, "parsing endpoint node failed\n");
		goto out_err;
	}

	if (!bus_cfg.nr_of_link_frequencies) {
		dev_err(dev, "no link frequencies defined");
		ret = -EINVAL;
		goto out_err;
	}

	for (i = 0; i < ARRAY_SIZE(ov05c10_link_freq_menu_items); i++) {
		for (j = 0; j < bus_cfg.nr_of_link_frequencies; j++) {
			if (ov05c10_link_freq_menu_items[i] ==
				bus_cfg.link_frequencies[j])
				break;
		}

		if (j == bus_cfg.nr_of_link_frequencies) {
			dev_err(dev, "no link frequency %lld supported",
				ov05c10_link_freq_menu_items[i]);
			ret = -EINVAL;
			goto out_err;
		}
	}

out_err:
	v4l2_fwnode_endpoint_free(&bus_cfg);

	return ret;
}

static int ov05c10_identify_module(struct ov05c10 *ov05c10)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov05c10->sd);
	int ret;
	u64 val_h, val_l;
	u32 val;

	ret = cci_write(ov05c10->regmap, REG_PAGE_FLAG, PAGE_0, NULL);
	if (ret) {
		dev_err(&client->dev, "chip page write err");
		return ret;
	}

	ret = cci_read(ov05c10->regmap, OV05C10_REG_CHIP_ID_H, &val_h, NULL);
	if (ret)
		return ret;
	ret = cci_read(ov05c10->regmap, OV05C10_REG_CHIP_ID_L, &val_l, NULL);
	if (ret)
		return ret;

	val = ((val_h << 16) | val_l);
	if (val != OV05C10_CHIP_ID) {
		dev_err(&client->dev, "chip id mismatch: %x!=%u",
			OV05C10_CHIP_ID, val);
		return -ENXIO;
	}
	return 0;
}

/* This function tries to get power control resources */
static int ov05c10_get_pm_resources(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ov05c10 *ov05c10 = to_ov05c10(sd);
	int ret;

	ov05c10->img_clk = devm_clk_get_optional(dev, NULL);
	if (IS_ERR(ov05c10->img_clk))
		return dev_err_probe(dev, PTR_ERR(ov05c10->img_clk),
				     "failed to get imaging clock\n");

	ov05c10->avdd = devm_regulator_get_optional(dev, "avdd");
	if (IS_ERR(ov05c10->avdd)) {
		ret = PTR_ERR(ov05c10->avdd);
		ov05c10->avdd = NULL;
		if (ret != -ENODEV)
			return dev_err_probe(dev, ret,
					     "failed to get avdd regulator\n");
	}

	ov05c10->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ov05c10->reset))
		return dev_err_probe(dev, PTR_ERR(ov05c10->reset),
				     "failed to get reset gpio\n");

	return 0;
}

static int ov05c10_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ov05c10 *ov05c10 = to_ov05c10(sd);

	if (ov05c10->reset)
		gpiod_set_value_cansleep(ov05c10->reset, 1);

	if (ov05c10->avdd)
		regulator_disable(ov05c10->avdd);

	clk_disable_unprepare(ov05c10->img_clk);

	return 0;
}

static int ov05c10_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ov05c10 *ov05c10 = to_ov05c10(sd);
	int ret;

	ret = clk_prepare_enable(ov05c10->img_clk);
	if (ret < 0) {
		dev_err(dev, "failed to enable imaging clock: %d", ret);
		return ret;
	}

	if (ov05c10->avdd) {
		ret = regulator_enable(ov05c10->avdd);
		if (ret < 0) {
			dev_err(dev, "failed to enable avdd: %d", ret);
			clk_disable_unprepare(ov05c10->img_clk);
			return ret;
		}
	}

	if (ov05c10->reset) {
		gpiod_set_value_cansleep(ov05c10->reset, 0);
		/* 5ms to wait ready after XSHUTDN assert */
		usleep_range(5000, 5500);
	}

	return 0;
}

static const struct dev_pm_ops ov05c10_pm_ops = {
	SET_RUNTIME_PM_OPS(ov05c10_power_off, ov05c10_power_on, NULL)
};

static int ov05c10_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ov05c10 *ov05c10;
	bool full_power;
	int ret;

	ov05c10 = devm_kzalloc(&client->dev, sizeof(*ov05c10), GFP_KERNEL);
	if (!ov05c10)
		return -ENOMEM;
	ret = ov05c10_parse_fwnode(ov05c10, dev);
	if (ret)
		return ret;

	ov05c10->regmap = devm_cci_regmap_init_i2c(client, 8);
	if (IS_ERR(ov05c10->regmap))
		return dev_err_probe(dev, PTR_ERR(ov05c10->regmap),
				     "failed to init CCI\n");
	v4l2_i2c_subdev_init(&ov05c10->sd, client, &ov05c10_subdev_ops);

	ret = ov05c10_get_pm_resources(dev);
	if (ret)
		return ret;

	full_power = acpi_dev_state_d0(dev);
	if (full_power) {
		ret = ov05c10_power_on(dev);
		if (ret) {
			dev_err(&client->dev, "failed to power on\n");
			return ret;
		}

		/* Check module identity */
		ret = ov05c10_identify_module(ov05c10);
		if (ret) {
			dev_err(dev, "failed to find sensor: %d\n", ret);
			goto error_power_off;
		}
	}

	ov05c10->cur_mode = &supported_modes[0];
	ret = ov05c10_init_controls(ov05c10);
	if (ret) {
		dev_err(&client->dev, "failed to init controls: %d", ret);
		goto probe_error_v4l2_ctrl_handler_free;
	}

	ov05c10->sd.internal_ops = &ov05c10_internal_ops;
	ov05c10->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
			V4L2_SUBDEV_FL_HAS_EVENTS;
	ov05c10->sd.entity.ops = &ov05c10_subdev_entity_ops;
	ov05c10->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ov05c10->pad.flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&ov05c10->sd.entity, 1, &ov05c10->pad);
	if (ret) {
		dev_err(&client->dev, "failed to init entity pads: %d", ret);
		goto probe_error_v4l2_ctrl_handler_free;
	}

	ov05c10->sd.state_lock = ov05c10->ctrl_handler.lock;
	ret = v4l2_subdev_init_finalize(&ov05c10->sd);
	if (ret < 0) {
		dev_err(dev, "v4l2 subdev init error: %d\n", ret);
		goto probe_error_media_entity_cleanup;
	}
	/*
	 * Device is already turned on by i2c-core with ACPI domain PM.
	 * Enable runtime PM and turn off the device.
	 */
	if (full_power)
		pm_runtime_set_active(&client->dev);
	pm_runtime_enable(&client->dev);
	pm_runtime_idle(&client->dev);
	ret = v4l2_async_register_subdev_sensor(&ov05c10->sd);
	if (ret < 0) {
		dev_err(&client->dev, "failed to register V4L2 subdev: %d",
			ret);
		goto probe_error_rpm;
	}
	return 0;

probe_error_rpm:
	pm_runtime_disable(&client->dev);
	v4l2_subdev_cleanup(&ov05c10->sd);
probe_error_media_entity_cleanup:
	media_entity_cleanup(&ov05c10->sd.entity);
probe_error_v4l2_ctrl_handler_free:
	v4l2_ctrl_handler_free(ov05c10->sd.ctrl_handler);
error_power_off:
	ov05c10_power_off(&client->dev);

	return ret;
}

static void ov05c10_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov05c10 *ov05c10 = to_ov05c10(sd);

	v4l2_async_unregister_subdev(&ov05c10->sd);
	v4l2_subdev_cleanup(sd);
	media_entity_cleanup(&ov05c10->sd.entity);
	v4l2_ctrl_handler_free(&ov05c10->ctrl_handler);
	pm_runtime_disable(&client->dev);

	if (!pm_runtime_status_suspended(&client->dev))
		ov05c10_power_off(&client->dev);
	pm_runtime_set_suspended(&client->dev);
}

static const struct acpi_device_id ov05c10_acpi_ids[] = {
	{ "OVTI05C1" },
	{}
};

MODULE_DEVICE_TABLE(acpi, ov05c10_acpi_ids);

static struct i2c_driver ov05c10_i2c_driver = {
	.driver = {
		.name  = "ov05c10",
		.pm = pm_ptr(&ov05c10_pm_ops),
		.acpi_match_table = ACPI_PTR(ov05c10_acpi_ids),
	},
	.probe = ov05c10_probe,
	.remove = ov05c10_remove,
};

module_i2c_driver(ov05c10_i2c_driver);
MODULE_DESCRIPTION("OmniVision ov05c10 camera driver");
MODULE_AUTHOR("Dongcheng Yan <dongcheng.yan@intel.com>");
MODULE_LICENSE("GPL");
