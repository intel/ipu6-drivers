// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2024 Intel Corporation

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/version.h>

#include <linux/ipu-isys.h>

#include <linux/gpio/driver.h>

#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/ti960.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>

#include "ti960-reg.h"
#include "ti953.h"

// FIXME: Move these to platform data
#define D3_BUS_SWITCH_ADDR 0x70
#define D3_GPIO_EXP_ADDR 0x70
#define D3_GPIO_EXP_ALIAS(rx_port) (0x60+rx_port)

#define NUM_ALIASES 8

#define MIPI_CSI2_TYPE_RAW12   0x2c
#define MIPI_CSI2_TYPE_YUV422_8	0x1e
#define SUFFIX_BASE 96

struct ti960_slave {
	unsigned short addr;
	unsigned short alias;
	bool auto_ack;
};

struct ti960_subdev {
	struct v4l2_subdev *sd;
	unsigned short rx_port;
	unsigned short fsin_gpio;
	unsigned short phy_i2c_addr;
	unsigned short alias_i2c_addr;
	unsigned short ser_i2c_addr;
	struct i2c_client *serializer;
	struct i2c_client *gpio_exp;
	struct ti960_slave slaves[NUM_ALIASES];
	char sd_name[16];
};

struct ti960 {
	struct v4l2_subdev sd;
	struct media_pad pad[NR_OF_TI960_PADS];
	struct v4l2_ctrl_handler ctrl_handler;
	struct ti960_pdata *pdata;
	struct ti960_subdev sub_devs[NR_OF_TI960_SINK_PADS];
	struct ti960_subdev_pdata subdev_pdata[NR_OF_TI960_SINK_PADS];
	struct i2c_client *bus_switch;
	const char *name;

	struct mutex mutex;

	struct regmap *regmap8;
	struct regmap *regmap16;

	struct v4l2_mbus_framefmt *ffmts[NR_OF_TI960_PADS];
	struct rect *crop;
	struct rect *compose;

	unsigned int nsinks;
	unsigned int nsources;
	unsigned int nstreams;
	unsigned int npads;

	struct gpio_chip gc;

	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *test_pattern;

};

#define to_ti960(_sd) container_of(_sd, struct ti960, sd)

static const s64 ti960_op_sys_clock[] =  {800000000,
					  600000000,
					  400000000,
					  200000000};

/*
 * Order matters.
 *
 * 1. Bits-per-pixel, descending.
 * 2. Bits-per-pixel compressed, descending.
 * 3. Pixel order, same as in pixel_order_str. Formats for all four pixel
 *    orders must be defined.
 */
static const struct ti960_csi_data_format va_csi_data_formats[] = {
	{ MEDIA_BUS_FMT_YUYV8_1X16, 16, 16, PIXEL_ORDER_GBRG, 0x1e },
	{ MEDIA_BUS_FMT_UYVY8_1X16, 16, 16, PIXEL_ORDER_GBRG, 0x1e },
	{ MEDIA_BUS_FMT_SGRBG16_1X16, 16, 16, PIXEL_ORDER_GRBG, 0x2e },
	{ MEDIA_BUS_FMT_SRGGB16_1X16, 16, 16, PIXEL_ORDER_RGGB, 0x2e },
	{ MEDIA_BUS_FMT_SBGGR16_1X16, 16, 16, PIXEL_ORDER_BGGR, 0x2e },
	{ MEDIA_BUS_FMT_SGBRG16_1X16, 16, 16, PIXEL_ORDER_GBRG, 0x2e },
	{ MEDIA_BUS_FMT_SGRBG12_1X12, 12, 12, PIXEL_ORDER_GRBG, 0x2c },
	{ MEDIA_BUS_FMT_SRGGB12_1X12, 12, 12, PIXEL_ORDER_RGGB, 0x2c },
	{ MEDIA_BUS_FMT_SBGGR12_1X12, 12, 12, PIXEL_ORDER_BGGR, 0x2c },
	{ MEDIA_BUS_FMT_SGBRG12_1X12, 12, 12, PIXEL_ORDER_GBRG, 0x2c },
	{ MEDIA_BUS_FMT_SGRBG10_1X10, 10, 10, PIXEL_ORDER_GRBG, 0x2b },
	{ MEDIA_BUS_FMT_SRGGB10_1X10, 10, 10, PIXEL_ORDER_RGGB, 0x2b },
	{ MEDIA_BUS_FMT_SBGGR10_1X10, 10, 10, PIXEL_ORDER_BGGR, 0x2b },
	{ MEDIA_BUS_FMT_SGBRG10_1X10, 10, 10, PIXEL_ORDER_GBRG, 0x2b },
};

static const uint32_t ti960_supported_codes_pad[] = {
	MEDIA_BUS_FMT_YUYV8_1X16,
	MEDIA_BUS_FMT_UYVY8_1X16,
	MEDIA_BUS_FMT_SBGGR16_1X16,
	MEDIA_BUS_FMT_SGBRG16_1X16,
	MEDIA_BUS_FMT_SGRBG16_1X16,
	MEDIA_BUS_FMT_SRGGB16_1X16,
	MEDIA_BUS_FMT_SBGGR12_1X12,
	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SGRBG12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,
	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	0,
};

static const uint32_t *ti960_supported_codes[] = {
	ti960_supported_codes_pad,
};

static struct regmap_config ti960_reg_config8 = {
	.reg_bits = 8,
	.val_bits = 8,
};

static struct regmap_config ti960_reg_config16 = {
	.reg_bits = 16,
	.val_bits = 8,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
};

static s64 ti960_query_sub_stream[NR_OF_TI960_DEVS][NR_OF_TI960_SINK_PADS] = {
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
};

static void set_sub_stream_fmt(int port, int index, u32 code)
{
	ti960_query_sub_stream[port][index] &= 0xFFFFFFFFFFFF0000;
	ti960_query_sub_stream[port][index] |= code;
}

static void set_sub_stream_h(int port, int index, u32 height)
{
	s64 val = height & 0xFFFF;

	ti960_query_sub_stream[port][index] &= 0xFFFFFFFF0000FFFF;
	ti960_query_sub_stream[port][index] |= val << 16;
}

static void set_sub_stream_w(int port, int index, u32 width)
{
	s64 val = width & 0xFFFF;

	ti960_query_sub_stream[port][index] &= 0xFFFF0000FFFFFFFF;
	ti960_query_sub_stream[port][index] |= val << 32;
}

static void set_sub_stream_dt(int port, int index, u32 dt)
{
	s64 val = dt & 0xFF;

	ti960_query_sub_stream[port][index] &= 0xFF00FFFFFFFFFFFF;
	ti960_query_sub_stream[port][index] |= val << 48;
}

static void set_sub_stream_vc_id(int port, int index, u32 vc_id)
{
	s64 val = vc_id & 0xFF;

	ti960_query_sub_stream[port][index] &= 0x00FFFFFFFFFFFFFF;
	ti960_query_sub_stream[port][index] |= val << 56;
}

static u8 ti960_set_sub_stream[NR_OF_TI960_DEVS][NR_OF_TI960_SINK_PADS] = {
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
};

static int bus_switch(struct ti960 *va)
{
	int ret;
	int retry, timeout = 10;
	struct i2c_client *client = va->bus_switch;

	if (!va->bus_switch)
		return 0;

	dev_dbg(&client->dev, "try to set bus switch\n");

	for (retry = 0; retry < timeout; retry++) {
		ret = i2c_smbus_write_byte(client, 0x01);
		if (ret < 0)
			usleep_range(5000, 6000);
		else
			break;
	}

	if (retry >= timeout) {
		dev_err(&client->dev, "bus switch failed, maybe no bus switch\n");
	}

	return 0;
}

static int ti960_reg_read(struct ti960 *va, unsigned char reg, unsigned int *val)
{
	int ret, retry, timeout = 10;

	for (retry = 0; retry < timeout; retry++) {
		ret = regmap_read(va->regmap8, reg, val);
		if (ret < 0) {
			dev_err(va->sd.dev, "960 reg read ret=%x", ret);
			usleep_range(5000, 6000);
		} else {
			break;
		}
	}

	if (retry >= timeout) {
		dev_err(va->sd.dev,
			"%s:devid read failed: reg=%2x, ret=%d\n",
			__func__, reg, ret);
		return -EREMOTEIO;
	}

	return 0;
}

static int ti960_reg_write(struct ti960 *va, unsigned char reg, unsigned int val)
{
	int ret, retry, timeout = 10;

	for (retry = 0; retry < timeout; retry++) {
		dev_dbg(va->sd.dev, "write reg %x = %x", reg, val);
		ret = regmap_write(va->regmap8, reg, val);
		if (ret < 0) {
			dev_err(va->sd.dev, "960 reg write ret=%x", ret);
			usleep_range(5000, 6000);
		} else {
			break;
		}
	}

	if (retry >= timeout) {
		dev_err(va->sd.dev,
			"%s:devid write failed: reg=%2x, ret=%d\n",
			__func__, reg, ret);
		return -EREMOTEIO;
	}

	return 0;
}

static int ti960_reg_set_bit(struct ti960 *va, unsigned char reg,
	unsigned char bit, unsigned char val)
{
	int ret;
	unsigned int reg_val;

	ret = regmap_read(va->regmap8, reg, &reg_val);
	if (ret)
		return ret;
	if (val)
		reg_val |= 1 << bit;
	else
		reg_val &= ~(1 << bit);

	return ti960_reg_write(va, reg, reg_val);
}

static int ti960_map_i2c_slave(struct ti960 *va, struct ti960_subdev *sd,
			       unsigned short addr, unsigned int alias_addr)
{
	int rval;
	int index;
	struct ti960_slave *slaves = sd->slaves;

	for (index = 0; index < NUM_ALIASES; index++)
		if (slaves[index].addr == 0)
			break;

	if (index >= NUM_ALIASES)
		return -ENOMEM;
	if (addr != 0)
		slaves[index].addr = addr;

	if (alias_addr != 0)
		slaves[index].alias = alias_addr;

	if (addr != 0 && alias_addr != 0)
		dev_dbg(va->sd.dev, "%s rx port %d: map %02x to alias %02x\n",
				__func__, sd->rx_port, addr, alias_addr);

	rval = ti960_reg_write(va, TI960_RX_PORT_SEL,
		(sd->rx_port << 4) + (1 << sd->rx_port));
	if (rval)
		return rval;

	if (addr != 0) {
		rval = ti960_reg_write(va, TI960_SLAVE_ID0 + index, addr << 1);
		if (rval)
			return rval;
	}

	if (alias_addr != 0) {
		rval = ti960_reg_write(va, TI960_SLAVE_ALIAS_ID0 + index, alias_addr << 1);
		if (rval)
			return rval;
	}

	return 0;
}

static int ti960_unmap_i2c_slave(struct ti960 *va, struct ti960_subdev *sd,
				 unsigned short addr)
{
	int rval;
	int index;
	struct ti960_slave *slaves = sd->slaves;

	for (index = 0; index < NUM_ALIASES; index++)
		if (slaves[index].addr == addr)
			break;

	if (index >= NUM_ALIASES)
		return 0;

	dev_dbg(va->sd.dev, "rx port %d: unmap %02x from alias %02x\n",
			sd->rx_port, addr, slaves[index].alias);
	slaves[index].addr = 0;
	slaves[index].alias = 0;

	rval = ti960_reg_write(va, TI960_SLAVE_ID0 + index, 0);
	if (rval)
		return rval;

	rval = ti960_reg_write(va, TI960_SLAVE_ALIAS_ID0 + index, 0);
	if (rval)
		return rval;

	return 0;
}

static int ti960_map_ser_alias_addr(struct ti960 *va, unsigned short rx_port,
			      unsigned short ser_alias)
{
	int rval;

	dev_err(va->sd.dev, "%s port %d, ser_alias %x\n", __func__, rx_port, ser_alias);
	rval = ti960_reg_write(va, TI960_RX_PORT_SEL,
		(rx_port << 4) + (1 << rx_port));
	if (rval)
		return rval;

	return ti960_reg_write(va, TI960_SER_ALIAS_ID, ser_alias << 1);
}

static int ti960_fsin_gpio_init(struct ti960 *va, struct ti960_subdev *subdev)
{
	unsigned char gpio_data;
	int rval;
	int reg_val;
	unsigned short rx_port = subdev->rx_port;
	unsigned short ser_alias = subdev->ser_i2c_addr;
	unsigned short fsin_gpio = subdev->fsin_gpio;
	struct i2c_client *serializer = subdev->serializer;

	dev_dbg(va->sd.dev, "%s\n", __func__);
	rval = regmap_read(va->regmap8, TI960_FS_CTL, &reg_val);
	if (rval) {
		dev_dbg(va->sd.dev, "Failed to read gpio status.\n");
		return rval;
	}

	if (!reg_val & TI960_FSIN_ENABLE) {
		dev_dbg(va->sd.dev, "FSIN not enabled, skip config FSIN GPIO.\n");
		return 0;
	}

	rval = ti960_reg_write(va, TI960_RX_PORT_SEL,
		(rx_port << 4) + (1 << rx_port));
	if (rval)
		return rval;

	switch (fsin_gpio) {
	case 0:
	case 1:
		rval = regmap_read(va->regmap8, TI960_BC_GPIO_CTL0, &reg_val);
		if (rval) {
			dev_dbg(va->sd.dev, "Failed to read gpio status.\n");
			return rval;
		}

		if (fsin_gpio == 0) {
			reg_val &= ~TI960_GPIO0_MASK;
			reg_val |= TI960_GPIO0_FSIN;
		} else {
			reg_val &= ~TI960_GPIO1_MASK;
			reg_val |= TI960_GPIO1_FSIN;
		}

		rval = ti960_reg_write(va, TI960_BC_GPIO_CTL0, reg_val);
		if (rval)
			dev_dbg(va->sd.dev, "Failed to set gpio.\n");
		break;
	case 2:
	case 3:
		rval = regmap_read(va->regmap8, TI960_BC_GPIO_CTL1, &reg_val);
		if (rval) {
			dev_dbg(va->sd.dev, "Failed to read gpio status.\n");
			return rval;
		}

		if (fsin_gpio == 2) {
			reg_val &= ~TI960_GPIO2_MASK;
			reg_val |= TI960_GPIO2_FSIN;
		} else {
			reg_val &= ~TI960_GPIO3_MASK;
			reg_val |= TI960_GPIO3_FSIN;
		}

		rval = ti960_reg_write(va, TI960_BC_GPIO_CTL1, reg_val);
		if (rval)
			dev_dbg(va->sd.dev, "Failed to set gpio.\n");
		break;
	}

	/* enable output and remote control */
	ti953_reg_write(serializer, TI953_GPIO_INPUT_CTRL, TI953_GPIO_OUT_EN);
	rval = ti953_reg_read(serializer, TI953_LOCAL_GPIO_DATA,
			&gpio_data);
	if (rval)
		return rval;
	ti953_reg_write(serializer, TI953_LOCAL_GPIO_DATA,
			gpio_data | TI953_GPIO0_RMTEN << fsin_gpio);

	return rval;
}

static int ti960_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_mbus_code_enum *code)
{
	const uint32_t *supported_code =
		ti960_supported_codes[code->pad];
	int i;

	for (i = 0; supported_code[i]; i++) {
		if (i == code->index) {
			code->code = supported_code[i];
			return 0;
		}
	}

	return -EINVAL;
}

static const struct ti960_csi_data_format
		*ti960_validate_csi_data_format(u32 code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(va_csi_data_formats); i++) {
		if (va_csi_data_formats[i].code == code)
			return &va_csi_data_formats[i];
	}

	return &va_csi_data_formats[0];
}

static int ti960_get_frame_desc(struct v4l2_subdev *sd,
	unsigned int pad, struct v4l2_mbus_frame_desc *desc)
{
	int sink_pad = pad;

	if (sink_pad >= 0) {
		struct media_pad *remote_pad =
			media_pad_remote_pad_first(&sd->entity.pads[sink_pad]);
		if (remote_pad) {
			struct v4l2_subdev *rsd = media_entity_to_v4l2_subdev(remote_pad->entity);

			dev_dbg(sd->dev, "%s remote sd: %s\n", __func__, rsd->name);
			v4l2_subdev_call(rsd, pad, get_frame_desc, 0, desc);
		}
	} else
		dev_err(sd->dev, "can't find the frame desc\n");

	return 0;
}

static struct v4l2_mbus_framefmt *
__ti960_get_ffmt(struct v4l2_subdev *subdev,
		 struct v4l2_subdev_state *sd_state,
		 unsigned int pad, unsigned int which,
		 unsigned int stream)
{
	struct ti960 *va = to_ti960(subdev);

	if (pad < 0 || pad >= NR_OF_TI960_PADS ||
	    stream < 0 || stream >= va->nstreams) {
		dev_err(subdev->dev, "%s invalid pad %d, or stream %d\n", __func__, pad, stream);
		return NULL;
	}

	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(subdev, sd_state, pad);
	else
		return &va->ffmts[pad][stream];
}

static int ti960_get_format(struct v4l2_subdev *subdev,
			    struct v4l2_subdev_state *sd_state,
			    struct v4l2_subdev_format *fmt)
{
	struct ti960 *va = to_ti960(subdev);
	struct v4l2_mbus_framefmt *ffmt;

	mutex_lock(&va->mutex);
	ffmt = __ti960_get_ffmt(subdev, sd_state, fmt->pad, fmt->which, 0);
	if (!ffmt) {
		mutex_unlock(&va->mutex);
		return -EINVAL;
	}
	fmt->format = *ffmt;
	mutex_unlock(&va->mutex);

	dev_dbg(subdev->dev, "subdev_format: which: %s, pad: %d.\n",
		 fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE ?
		 "V4L2_SUBDEV_FORMAT_ACTIVE" : "V4L2_SUBDEV_FORMAT_TRY",
		 fmt->pad);

	dev_dbg(subdev->dev, "framefmt: width: %d, height: %d, code: 0x%x.\n",
	       fmt->format.width, fmt->format.height, fmt->format.code);

	return 0;
}

static int ti960_set_format(struct v4l2_subdev *subdev,
			    struct v4l2_subdev_state *sd_state,
			    struct v4l2_subdev_format *fmt)
{
	struct ti960 *va = to_ti960(subdev);
	const struct ti960_csi_data_format *csi_format;
	struct v4l2_mbus_framefmt *ffmt;
	u8 port = va->pdata->suffix - SUFFIX_BASE;

	csi_format = ti960_validate_csi_data_format(
		fmt->format.code);

	mutex_lock(&va->mutex);
	ffmt = __ti960_get_ffmt(subdev, sd_state, fmt->pad, fmt->which, 0);

	if (!ffmt) {
		mutex_unlock(&va->mutex);
		return -EINVAL;
	}
	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		ffmt->width = fmt->format.width;
		ffmt->height = fmt->format.height;
		ffmt->code = csi_format->code;
	}
	fmt->format = *ffmt;

	if (fmt->pad < NR_OF_TI960_SINK_PADS) {
		set_sub_stream_fmt(port, fmt->pad, ffmt->code);
		set_sub_stream_h(port, fmt->pad, ffmt->height);
		set_sub_stream_w(port, fmt->pad, ffmt->width);

		/* select correct csi-2 data type id */
		if (ffmt->code >= MEDIA_BUS_FMT_UYVY8_1X16 &&
				ffmt->code <= MEDIA_BUS_FMT_YVYU8_1X16)
			set_sub_stream_dt(port, fmt->pad, MIPI_CSI2_TYPE_YUV422_8);
		else
			set_sub_stream_dt(port, fmt->pad, MIPI_CSI2_TYPE_RAW12);
		set_sub_stream_vc_id(port, fmt->pad, fmt->pad);
		dev_dbg(subdev->dev,
			"framefmt: width: %d, height: %d, code: 0x%x.\n",
			ffmt->width, ffmt->height, ffmt->code);
	}

	mutex_unlock(&va->mutex);
	return 0;
}

static int ti960_open(struct v4l2_subdev *subdev,
				struct v4l2_subdev_fh *fh)
{
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(subdev, fh->state, 0);

	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
		.pad = TI960_PAD_SOURCE,
		.format = {
			.width = TI960_MAX_WIDTH,
			.height = TI960_MAX_HEIGHT,
			.code = MEDIA_BUS_FMT_SBGGR12_1X12,
		},
	};

	*try_fmt = fmt.format;

	return 0;
}

static int ti960_map_subdevs_addr(struct ti960 *va)
{
	unsigned short rx_port, phy_i2c_addr, alias_i2c_addr;
	int i, rval;

	for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
		rx_port = va->sub_devs[i].rx_port;
		phy_i2c_addr = va->sub_devs[i].phy_i2c_addr;
		alias_i2c_addr = va->sub_devs[i].alias_i2c_addr;

		if (!phy_i2c_addr || !alias_i2c_addr)
			continue;

		rval = ti960_map_i2c_slave(va, &va->sub_devs[i],
				phy_i2c_addr, alias_i2c_addr);
		if (rval)
			return rval;
	}

	return 0;
}

static int ti960_unmap_subdevs_addr(struct ti960 *va)
{
	unsigned short rx_port, phy_i2c_addr;
	int i, rval;

	for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
		rx_port = va->sub_devs[i].rx_port;
		phy_i2c_addr = va->sub_devs[i].phy_i2c_addr;

		if (!phy_i2c_addr)
			continue;

		rval = ti960_unmap_i2c_slave(va, &va->sub_devs[i],
				phy_i2c_addr);
		if (rval) {
			dev_err(va->sd.dev, "%s  failed to unmap subdev: %s on port %d\n",
					va->sub_devs->sd_name, rx_port);
			return rval;
		}
	}

	return 0;
}

static int gpio_exp_reset_sensor(struct i2c_client *client, int reset)
{
	s32 gpio_cfg;
	s32 gpio_out;

	dev_dbg(&client->dev, "reset gpio %d", reset);

	gpio_cfg = i2c_smbus_read_byte_data(client, 0x03);
	if (gpio_cfg < 0)
		return gpio_cfg;
	gpio_cfg &= ~(1u << reset);
	i2c_smbus_write_byte_data(client, 0x03, gpio_cfg);

	gpio_out = i2c_smbus_read_byte_data(client, 0x01);
	gpio_out &= ~(1u << reset);
	i2c_smbus_write_byte_data(client, 0x01, gpio_out);
	msleep(50);
	gpio_out |= (1u << reset);
	i2c_smbus_write_byte_data(client, 0x01, gpio_out);

	return 0;
}

/*
 * FIXME: workaround, reset to avoid block.
 */
static int ti953_reset_sensor(struct i2c_client *client, int reset)
{
	int rval;
	unsigned char gpio_data;

	rval = ti953_reg_read(client, TI953_LOCAL_GPIO_DATA,
			&gpio_data);
	if (rval)
		return rval;

	ti953_reg_write(client, TI953_GPIO_INPUT_CTRL,
			TI953_GPIO_OUT_EN);
	gpio_data &= ~(TI953_GPIO0_RMTEN << reset);
	gpio_data &= ~(TI953_GPIO0_OUT << reset);
	ti953_reg_write(client, TI953_LOCAL_GPIO_DATA,
			gpio_data);
	msleep(50);
	gpio_data |= TI953_GPIO0_OUT << reset;
	ti953_reg_write(client, TI953_LOCAL_GPIO_DATA,
			gpio_data);
	msleep(50);
	return 0;
}

static int reset_sensor(struct ti960 *va, struct ti960_subdev *sd,
			struct ti960_subdev_pdata *pdata)
{
	if (sd->gpio_exp)
		return gpio_exp_reset_sensor(sd->gpio_exp, pdata->reset);

	if (sd->serializer)
		return ti953_reset_sensor(sd->serializer, pdata->reset);

	return -ENODEV;
}

static bool gpio_exp_detect(struct i2c_client *client)
{
	int rval;

	rval = i2c_smbus_read_byte_data(client, 0x03);
	if (rval < 0)
		return false;

	return (rval == 0xFF);
}

static int ti960_config_ser(struct ti960 *va, struct i2c_client *client, int k,
			    struct ti960_subdev *subdev,
			    struct ti960_subdev_pdata *pdata)
{
	unsigned short rx_port, phy_i2c_addr, alias_i2c_addr, ser_i2c_addr;
	int i, rval;
	unsigned char val;
	bool speed_detect_fail;
	int timeout = 50;

	rx_port = subdev->rx_port;
	phy_i2c_addr = subdev->phy_i2c_addr;
	alias_i2c_addr = subdev->alias_i2c_addr;
	ser_i2c_addr = subdev->ser_i2c_addr;

	rval = ti960_map_ser_alias_addr(va, rx_port,
			ser_i2c_addr);
	if (rval)
		return rval;

	subdev->serializer = i2c_new_dummy_device(client->adapter, ser_i2c_addr);
	if (IS_ERR(subdev->serializer)) {
		dev_err(va->sd.dev, "rx port %d: Failed to allocate serializer client: %ld",
				rx_port, PTR_ERR(subdev->serializer));
		return -ENODEV;
	}

	if (!ti953_detect(subdev->serializer)) {
		dev_warn(va->sd.dev, "rx port %d: No link detected",
				rx_port);
		i2c_unregister_device(subdev->serializer);
		subdev->serializer = NULL;
		return -ENODEV;
	}
	dev_info(va->sd.dev, "rx port: %d: Found serializer", rx_port);
	va->sub_devs[k].serializer = subdev->serializer;

	rval = ti960_map_i2c_slave(va, &va->sub_devs[k],
					D3_GPIO_EXP_ADDR, D3_GPIO_EXP_ALIAS(rx_port));
	if (rval)
		return rval;

	subdev->gpio_exp = i2c_new_dummy_device(client->adapter, D3_GPIO_EXP_ALIAS(rx_port));
	if (IS_ERR(subdev->gpio_exp)) {
		dev_err(va->sd.dev, "rx port %d: Failed to allocate gpio-expander client: %ld",
				rx_port, PTR_ERR(subdev->gpio_exp));
	} else {
		if (!gpio_exp_detect(subdev->gpio_exp)) {
			dev_err(va->sd.dev, "rx port: %d: Missing gpio-expander", rx_port);
			i2c_unregister_device(subdev->gpio_exp);
			subdev->gpio_exp = NULL;
		} else {
			dev_info(va->sd.dev, "rx port: %d: Found gpio-expander", rx_port);
			va->sub_devs[k].gpio_exp = subdev->gpio_exp;
		}
	}

	ti953_reg_write(subdev->serializer,
			TI953_RESET_CTL, TI953_DIGITAL_RESET_1);

	/*
	 * ti953 pull down time is at least 3ms
	 * add 2ms more as buffer
	 */
	while (timeout--) {
		rval = ti953_reg_read(subdev->serializer, TI953_DEVICE_ID, &val);
		if ((val == 0x30) || (val == 0x32))
			break;

		usleep_range(100, 110);
	}
	if (timeout == 0) {
		dev_err(va->sd.dev, "ti953 pull down timeout.\n");
	} else {
		dev_info(va->sd.dev, "ti953 pull down succeed, loop time %d.\n", (50 - timeout));
	}

	if (pdata->module_flags & TI960_FL_INIT_SER) {
		rval = ti953_init(subdev->serializer);
		if (rval)
			return rval;
	}

	if (pdata->module_flags & TI960_FL_INIT_SER_CLK) {
		rval = ti953_init_clk(subdev->serializer);
		if (rval)
			return rval;
	}

	if (pdata->module_flags & TI960_FL_POWERUP) {
		ti953_reg_write(subdev->serializer,
				TI953_GPIO_INPUT_CTRL, TI953_GPIO_OUT_EN);

		/* boot sequence */
		for (i = 0; i < TI960_MAX_GPIO_POWERUP_SEQ; i++) {
			if (pdata->gpio_powerup_seq[i] == (char)-1)
				break;
			ti953_reg_write(subdev->serializer,
					TI953_LOCAL_GPIO_DATA,
					pdata->gpio_powerup_seq[i]);
		}
	}

	/* Configure ti953 CSI lane */
	rval = ti953_reg_read(subdev->serializer,
			      TI953_GENERAL_CFG, &val);
	dev_dbg(va->sd.dev, "ti953 read default general CFG (%x)\n", val);
	if (va->pdata->ser_nlanes == 2)
		val |= TI953_CSI_2LANE;
	else if (va->pdata->ser_nlanes == 4)
		val |= TI953_CSI_4LANE;
	else
		dev_err(va->sd.dev, "not expected csi lane\n");
	rval = ti953_reg_write(subdev->serializer,
			       TI953_GENERAL_CFG, val);
	if (rval != 0) {
		dev_err(va->sd.dev, "ti953 write failed(%d)\n", rval);
		return rval;
	}

	ti953_bus_speed(subdev->serializer,
			TI953_I2C_SPEED_FAST_PLUS);
	speed_detect_fail =
		ti953_reg_read(subdev->serializer, 0, &val);
	if (speed_detect_fail) {
		ti953_bus_speed(subdev->serializer,
				TI953_I2C_SPEED_FAST);
		speed_detect_fail =
			ti953_reg_read(subdev->serializer, 0, &val);
	}
	if (speed_detect_fail) {
		ti953_bus_speed(subdev->serializer,
				TI953_I2C_SPEED_STANDARD);
		speed_detect_fail =
			ti953_reg_read(subdev->serializer, 0, &val);
	}
	if (speed_detect_fail)
		dev_err(va->sd.dev, "i2c bus speed standard failed!");
	return 0;
}

static int ti960_registered(struct v4l2_subdev *subdev)
{
	struct ti960 *va = to_ti960(subdev);
	struct i2c_client *client = v4l2_get_subdevdata(subdev);
	int i, j, k, l, rval;
	bool port_registered[NR_OF_TI960_SINK_PADS];

	for (i = 0 ; i < NR_OF_TI960_SINK_PADS; i++)
		port_registered[i] = false;
	for (i = 0, k = 0; i < va->pdata->subdev_num; i++) {
		struct ti960_subdev_info *info =
			&va->pdata->subdev_info[i];
		struct ti960_subdev_pdata *pdata =
			(struct ti960_subdev_pdata *)
			info->board_info.platform_data;
		struct ti960_subdev sensor_subdev;
		sensor_subdev.rx_port = info->rx_port;
		sensor_subdev.phy_i2c_addr = info->phy_i2c_addr;
		sensor_subdev.alias_i2c_addr = info->board_info.addr;
		sensor_subdev.ser_i2c_addr = info->ser_alias;
		if (k >= va->nsinks)
			break;

		if (port_registered[info->rx_port]) {
			dev_err(va->sd.dev,
				"rx port %d registed already\n",
				info->rx_port);
			continue;
		}

		/*
		 * The sensors should not share the same pdata structure.
		 * Clone the pdata for each sensor.
		 */
		memcpy(&va->subdev_pdata[k], pdata, sizeof(*pdata));

		va->sub_devs[k].fsin_gpio = va->subdev_pdata[k].fsin;
		va->sub_devs[k].rx_port = info->rx_port;
		/* Spin sensor subdev suffix name */
		va->subdev_pdata[k].suffix = info->suffix;

		/*
		 * Change the gpio value to have xshutdown
		 * and rx port included, so in gpio_set those
		 * can be caculated from it.
		 */
		va->subdev_pdata[k].xshutdown += va->gc.base +
					info->rx_port * NR_OF_GPIOS_PER_PORT;
		info->board_info.platform_data = &va->subdev_pdata[k];

		if (!info->phy_i2c_addr || !info->board_info.addr) {
			dev_err(va->sd.dev, "can't find the physical and alias addr.\n");
			return -EINVAL;
		}
		rval = ti960_config_ser(va, client, k, &sensor_subdev, pdata);
		if (rval) {
			dev_warn(va->sd.dev, "resume: failed config subdev");
			continue;
		}

		rval = reset_sensor(va, &va->sub_devs[k], &va->subdev_pdata[k]);
		if (rval) {
			dev_err(va->sd.dev, "sensor failed to reset.\n");
			continue;
		}

		/* Map slave I2C address. */
		rval = ti960_map_i2c_slave(va, &va->sub_devs[k],
				info->phy_i2c_addr, info->board_info.addr);
		if (rval)
			continue;

		va->sub_devs[k].sd = v4l2_i2c_new_subdev_board(
			va->sd.v4l2_dev, client->adapter,
			&info->board_info, 0);
		if (!va->sub_devs[k].sd) {
			dev_err(va->sd.dev,
				"can't create new i2c subdev %c\n",
				info->suffix);
			continue;
		}
		va->sub_devs[k].phy_i2c_addr = info->phy_i2c_addr;
		va->sub_devs[k].alias_i2c_addr = info->board_info.addr;
		va->sub_devs[k].ser_i2c_addr = info->ser_alias;
		snprintf(va->sub_devs[k].sd->name, sizeof(va->sd.name), "%s %c",
			va->subdev_pdata[k].module_name,
			va->subdev_pdata[k].suffix);
		memcpy(va->sub_devs[k].sd_name,
				va->subdev_pdata[k].module_name,
				min(sizeof(va->sub_devs[k].sd_name) - 1,
				sizeof(va->subdev_pdata[k].module_name) - 1));

		for (j = 0; j < va->sub_devs[k].sd->entity.num_pads; j++) {
			if (va->sub_devs[k].sd->entity.pads[j].flags &
				MEDIA_PAD_FL_SOURCE)
				break;
		}

		if (j == va->sub_devs[k].sd->entity.num_pads) {
			dev_warn(va->sd.dev,
				"no source pad in subdev %c\n",
				info->suffix);
			return -ENOENT;
		}

		for (l = 0; l < va->nsinks; l++) {
			rval = media_create_pad_link(
			&va->sub_devs[k].sd->entity, j,
			&va->sd.entity, l, MEDIA_LNK_FL_DYNAMIC);
			if (rval) {
				dev_err(va->sd.dev,
					"can't create link to %c\n",
					info->suffix);
				return -EINVAL;
			}
		}
		port_registered[va->sub_devs[k].rx_port] = true;
		k++;
	}
	rval = ti960_map_subdevs_addr(va);
	if (rval)
		return rval;

	return 0;
}

static int ti960_set_power(struct v4l2_subdev *subdev, int on)
{
	struct ti960 *va = to_ti960(subdev);
	int ret;
	u8 val;
	u8 link_freq;

	ret = ti960_reg_write(va, TI960_RESET,
			   (on) ? TI960_POWER_ON : TI960_POWER_OFF);
	if (ret || !on)
		return ret;

	/* Select TX port 0 R/W by default */
	ret = ti960_reg_write(va, TI960_CSI_PORT_SEL, 0x01);
	/* Configure MIPI clock bsaed on control value. */
	link_freq = v4l2_ctrl_g_ctrl(va->link_freq);
	ret = ti960_reg_write(va, TI960_CSI_PLL_CTL,
			    link_freq);
	if (ret)
		return ret;
	val = TI960_CSI_ENABLE;

	/* Configure ti960 CSI lane */
	if (va->pdata->deser_nlanes == 2)
		val |= TI960_CSI_2LANE;
	else if (va->pdata->deser_nlanes == 4)
		val |= TI960_CSI_4LANE;
	else
		dev_err(va->sd.dev, "not expected csi lane\n");

	/* Enable skew calculation when 1.6Gbps output is enabled. */
	if (link_freq == TI960_MIPI_1600MBPS)
		val |= TI960_CSI_SKEWCAL;

	return ti960_reg_write(va, TI960_CSI_CTL, val);
}

static bool ti960_broadcast_mode(struct v4l2_subdev *subdev)
{
	struct ti960 *va = to_ti960(subdev);
	struct v4l2_subdev_format fmt = { 0 };
	struct v4l2_subdev *sd;
	char *sd_name = NULL;
	bool first = true;
	unsigned int h = 0, w = 0, code = 0;
	bool single_stream = true;
	int i, rval;
	u8 port = va->pdata->suffix - SUFFIX_BASE;

	for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
		struct media_pad *remote_pad =
			media_pad_remote_pad_first(&va->pad[i]);

		if (!remote_pad)
			continue;

		if (!ti960_set_sub_stream[port][i])
			continue;

		sd = media_entity_to_v4l2_subdev(remote_pad->entity);
		fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		fmt.pad = remote_pad->index;

		rval = v4l2_subdev_call(sd, pad, get_fmt, NULL, &fmt);
		if (rval)
			return false;

		if (first) {
			sd_name = va->sub_devs[i].sd_name;
			h = fmt.format.height;
			w = fmt.format.width;
			code = fmt.format.code;
			first = false;
		} else {
			if (strncmp(sd_name, va->sub_devs[i].sd_name, 16))
				return false;

			if (h != fmt.format.height || w != fmt.format.width
				|| code != fmt.format.code)
				return false;

			single_stream = false;
		}
	}

	if (single_stream)
		return false;

	return true;
}

static int ti960_rx_port_config(struct ti960 *va, int sink, int rx_port)
{
	int rval;
	unsigned int csi_vc_map;

	/* Select RX port. */
	rval = ti960_reg_write(va, TI960_RX_PORT_SEL,
			(rx_port << 4) + (1 << rx_port));
	if (rval) {
		dev_err(va->sd.dev, "Failed to select RX port.\n");
		return rval;
	}

	rval = ti960_reg_write(va, TI960_PORT_CONFIG,
		TI960_FPD3_CSI);
	if (rval) {
		dev_err(va->sd.dev, "Failed to set port config.\n");
		return rval;
	}

	/*
	 * CSI VC MAPPING.
	 */
	csi_vc_map = sink * 0x55;
	dev_info(va->sd.dev, "%s sink pad %d, rx_port %d, csi_vc_map %x",
		 __func__, sink, rx_port, csi_vc_map);
	rval = ti960_reg_write(va, TI960_CSI_VC_MAP,
			       csi_vc_map);
	if (rval) {
		dev_err(va->sd.dev, "Failed to set port config.\n");
		return rval;
	}
	return 0;
}

static int ti960_find_subdev_index(struct ti960 *va, struct v4l2_subdev *sd)
{
	int i;

	for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
		if (va->sub_devs[i].sd == sd)
			return i;
	}

	WARN_ON(1);

	return -EINVAL;
}

static int ti960_find_subdev_index_by_rx_port(struct ti960 *va, u8 rx_port)
{
	int i;

	for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
		if (va->sub_devs[i].rx_port == rx_port)
			return i;
	}
	WARN_ON(1);

	return -EINVAL;
}

static int ti960_set_frame_sync(struct ti960 *va, int enable)
{
	int i, rval;
	int index = !!enable;

	for (i = 0; i < ARRAY_SIZE(ti960_frame_sync_settings[index]); i++) {
		rval = ti960_reg_write(va,
				ti960_frame_sync_settings[index][i].reg,
				ti960_frame_sync_settings[index][i].val);
		if (rval) {
			dev_err(va->sd.dev, "Failed to %s frame sync\n",
				enable ? "enable" : "disable");
			return rval;
		}
	}
	dev_info(va->sd.dev, "Succeed to %s frame sync\n",
				enable ? "enable" : "disable");
	return 0;
}

static int ti960_set_stream(struct v4l2_subdev *subdev, int enable)
{
	struct ti960 *va = to_ti960(subdev);
	struct v4l2_subdev *sd;
	int i, j, rval;
	bool broadcast;
	unsigned short rx_port;
	unsigned short ser_alias;
	int sd_idx = -1;
	u8 port = va->pdata->suffix - SUFFIX_BASE;

	DECLARE_BITMAP(rx_port_enabled, 32);

	dev_dbg(va->sd.dev, "TI960 set stream, enable %d\n", enable);

	broadcast = ti960_broadcast_mode(subdev);
	if (enable)
		dev_info(va->sd.dev, "TI960 in %s mode",
			broadcast ? "broadcast" : "non broadcast");

	bitmap_zero(rx_port_enabled, 32);
	for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
		struct media_pad *remote_pad =
			media_pad_remote_pad_first(&va->pad[i]);

		if (!remote_pad)
			continue;

		if (!ti960_set_sub_stream[port][i])
			continue;

		/* Find ti960 subdev */
		sd = media_entity_to_v4l2_subdev(remote_pad->entity);
		j = ti960_find_subdev_index(va, sd);
		if (j < 0)
			return -EINVAL;
		rx_port = va->sub_devs[j].rx_port;
		ser_alias = va->sub_devs[j].ser_i2c_addr;
		rval = ti960_rx_port_config(va, i, rx_port);
		if (rval < 0)
			return rval;

		bitmap_set(rx_port_enabled, rx_port, 1);

		if (broadcast && sd_idx == -1) {
			sd_idx = j;
		} else if (broadcast) {
			rval = ti960_map_i2c_slave(va, rx_port, 0,
				va->sub_devs[sd_idx].alias_i2c_addr);
			if (rval < 0)
				return rval;
		} else {
			/* Stream on/off sensor */
			dev_err(va->sd.dev,
					"set stream for %s, enable  %d\n",
					sd->name, enable);
			rval = v4l2_subdev_call(sd, video, s_stream, enable);
			if (rval) {
				dev_err(va->sd.dev,
					"Failed to set stream for %s, enable  %d\n",
					sd->name, enable);
				return rval;
			}

			/* RX port fordward */
			rval = ti960_reg_set_bit(va, TI960_FWD_CTL1,
						rx_port + 4, !enable);
			if (rval) {
				dev_err(va->sd.dev,
					"Failed to forward RX port%d. enable %d\n",
					i, enable);
				return rval;
			}
			if (va->subdev_pdata[j].module_flags & TI960_FL_RESET) {
				rval = reset_sensor(va, &va->sub_devs[j], &va->subdev_pdata[j]);
				if (rval)
					return rval;
			}
		}
	}

	if (broadcast) {
		if (sd_idx < 0) {
			dev_err(va->sd.dev, "No sensor connected!\n");
			return -ENODEV;
		}
		sd = va->sub_devs[sd_idx].sd;
		rval = v4l2_subdev_call(sd, video, s_stream, enable);
		if (rval) {
			dev_err(va->sd.dev,
				"Failed to set stream for %s. enable  %d\n",
				sd->name, enable);
			return rval;
		}

		rval = ti960_set_frame_sync(va, enable);
		if (rval) {
			dev_err(va->sd.dev,
				"Failed to set frame sync.\n");
			return rval;
		}

		for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
			if (enable && test_bit(i, rx_port_enabled)) {
				rval = ti960_fsin_gpio_init(va, &va->sub_devs[i]);
				if (rval) {
					dev_err(va->sd.dev,
						"Failed to enable frame sync gpio init.\n");
					return rval;
				}

				if (va->subdev_pdata[i].module_flags & TI960_FL_RESET) {
					rx_port = va->sub_devs[i].rx_port;
					ser_alias = va->sub_devs[i].ser_i2c_addr;
					rval = reset_sensor(va, &va->sub_devs[i],
							&va->subdev_pdata[i]);
					if (rval)
						return rval;
				}
			}
		}

		for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
			if (!test_bit(i, rx_port_enabled))
				continue;

			/* RX port fordward */
			rval = ti960_reg_set_bit(va, TI960_FWD_CTL1,
						i + 4, !enable);
			if (rval) {
				dev_err(va->sd.dev,
					"Failed to forward RX port%d. enable %d\n",
					i, enable);
				return rval;
			}
		}
		/*
		 * Restore each subdev i2c address as we may
		 * touch it later.
		 */
		rval = ti960_map_subdevs_addr(va);
		if (rval)
			return rval;
	}

	return 0;
}

static int ti960_set_stream_vc(struct ti960 *va, u8 vc_id, u8 state)
{
	unsigned short rx_port;
	unsigned short ser_alias;
	struct v4l2_subdev *sd;
	int rval;
	int i;

	rval = ti960_reg_write(va, TI960_RESET, TI960_POWER_ON);
	if (rval < 0)
		return rval;

	i = ti960_find_subdev_index_by_rx_port(va, vc_id);
	if (i < 0)
		return -EINVAL;
	rx_port = va->sub_devs[i].rx_port;
	ser_alias = va->sub_devs[i].ser_i2c_addr;
	sd = va->sub_devs[i].sd;

	rval = ti960_rx_port_config(va, vc_id, rx_port);
	if (rval < 0)
		return rval;

	rval = v4l2_subdev_call(sd, video, s_stream, state);
	if (rval) {
		dev_err(va->sd.dev,
				"Failed to set stream for %s, enable %d\n",
				sd->name, state);
		return rval;
	}
	dev_info(va->sd.dev, "set stream for %s, enable %d\n",
			sd->name, state);

	/* RX port fordward */
	rval = ti960_reg_set_bit(va, TI960_FWD_CTL1,
			rx_port + 4, !state);
	if (rval) {
		dev_err(va->sd.dev,
				"Failed to forward RX port%d. enable %d\n",
				i, state);
		return rval;
	}

	rval = ti960_fsin_gpio_init(va, &va->sub_devs[i]);
	if (rval) {
		dev_err(va->sd.dev,
			"Failed to enable frame sync gpio init.\n");
		return rval;
	}

	if (va->subdev_pdata[i].module_flags & TI960_FL_RESET) {
		rval = reset_sensor(va, &va->sub_devs[i], &va->subdev_pdata[i]);
		if (rval)
			return rval;
	}

	return 0;
}

static struct v4l2_subdev_internal_ops ti960_sd_internal_ops = {
	.open = ti960_open,
	.registered = ti960_registered,
};

static const struct v4l2_subdev_video_ops ti960_sd_video_ops = {
	.s_stream = ti960_set_stream,
};

static const struct v4l2_subdev_core_ops ti960_core_subdev_ops = {
	.s_power = ti960_set_power,
};

static u8 ti960_get_nubmer_of_streaming(struct ti960 *va, u8 port)
{
	u8 n = 0;
	u8 i = 0;
	mutex_lock(&va->mutex);
	for (; i < ARRAY_SIZE(ti960_set_sub_stream[port]); i++) {
		if (ti960_set_sub_stream[port][i])
			n++;
	}
	mutex_unlock(&va->mutex);
	return n;
}

static int ti960_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ti960 *va = container_of(ctrl->handler,
					     struct ti960, ctrl_handler);
	u32 val;
	u8 vc_id;
	u8 state;
	u8 port = va->pdata->suffix - SUFFIX_BASE;

	switch (ctrl->id) {
	case V4L2_CID_IPU_SET_SUB_STREAM:
		val = (*ctrl->p_new.p_s64 & 0xFFFF);
		dev_info(va->sd.dev, "V4L2_CID_IPU_SET_SUB_STREAM %x\n", val);
		vc_id = (val >> 8) & 0x00FF;
		state = val & 0x00FF;
		if (vc_id > NR_OF_TI960_SINK_PADS - 1) {
			dev_err(va->sd.dev, "invalid vc %d\n", vc_id);
			break;
		}

		ti960_reg_write(va, TI960_CSI_PORT_SEL, 0x01);
		ti960_reg_read(va, TI960_CSI_CTL, &val);
		if (state) {
			if (ti960_get_nubmer_of_streaming(va, port) == 0)
				val |= TI960_CSI_CONTS_CLOCK;
			ti960_set_sub_stream[port][vc_id] = state;
		} else {
			ti960_set_sub_stream[port][vc_id] = state;
			if (ti960_get_nubmer_of_streaming(va, port) == 0)
				val &= ~TI960_CSI_CONTS_CLOCK;
		}

		ti960_reg_write(va, TI960_CSI_CTL, val);
		ti960_set_stream_vc(va, vc_id, state);
		break;
	default:
		dev_info(va->sd.dev, "unknown control id: 0x%X\n", ctrl->id);
	}

	return 0;
}

static const struct v4l2_ctrl_ops ti960_ctrl_ops = {
	.s_ctrl = ti960_s_ctrl,
};

static const struct v4l2_ctrl_config ti960_basic_controls[] = {
	{
		.ops = &ti960_ctrl_ops,
		.id = V4L2_CID_LINK_FREQ,
		.name = "V4L2_CID_LINK_FREQ",
		.type = V4L2_CTRL_TYPE_INTEGER_MENU,
		.min = 0,
		.max = ARRAY_SIZE(ti960_op_sys_clock) - 1,
		.def = TI960_MIPI_1200MBPS,
		.menu_skip_mask = 0,
		.qmenu_int = ti960_op_sys_clock,
	},
	{
		.ops = &ti960_ctrl_ops,
		.id = V4L2_CID_TEST_PATTERN,
		.name = "V4L2_CID_TEST_PATTERN",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 0,
	},
	{
		.ops = &ti960_ctrl_ops,
		.id = V4L2_CID_IPU_SET_SUB_STREAM,
		.name = "set virtual channel",
		.type = V4L2_CTRL_TYPE_INTEGER64,
		.max = 0xFFFF,
		.min = 0,
		.def = 0,
		.step = 1,
	},
};

static const struct v4l2_ctrl_config ti960_query_sub_control[NR_OF_TI960_DEVS] = {
	{
		.ops = &ti960_ctrl_ops,
		.id = V4L2_CID_IPU_QUERY_SUB_STREAM,
		.name = "query virtual channel",
		.type = V4L2_CTRL_TYPE_INTEGER_MENU,
		.max = ARRAY_SIZE(ti960_query_sub_stream[0]) - 1,
		.min = 0,
		.def = 0,
		.menu_skip_mask = 0,
		.qmenu_int = ti960_query_sub_stream[0],
	},
	{
		.ops = &ti960_ctrl_ops,
		.id = V4L2_CID_IPU_QUERY_SUB_STREAM,
		.name = "query virtual channel",
		.type = V4L2_CTRL_TYPE_INTEGER_MENU,
		.max = ARRAY_SIZE(ti960_query_sub_stream[1]) - 1,
		.min = 0,
		.def = 0,
		.menu_skip_mask = 0,
		.qmenu_int = ti960_query_sub_stream[1],
	},
	{
		.ops = &ti960_ctrl_ops,
		.id = V4L2_CID_IPU_QUERY_SUB_STREAM,
		.name = "query virtual channel",
		.type = V4L2_CTRL_TYPE_INTEGER_MENU,
		.max = ARRAY_SIZE(ti960_query_sub_stream[2]) - 1,
		.min = 0,
		.def = 0,
		.menu_skip_mask = 0,
		.qmenu_int = ti960_query_sub_stream[2],
	},
	{
		.ops = &ti960_ctrl_ops,
		.id = V4L2_CID_IPU_QUERY_SUB_STREAM,
		.name = "query virtual channel",
		.type = V4L2_CTRL_TYPE_INTEGER_MENU,
		.max = ARRAY_SIZE(ti960_query_sub_stream[3]) - 1,
		.min = 0,
		.def = 0,
		.menu_skip_mask = 0,
		.qmenu_int = ti960_query_sub_stream[3],
	},

};

static const struct v4l2_subdev_pad_ops ti960_sd_pad_ops = {
	.get_fmt = ti960_get_format,
	.set_fmt = ti960_set_format,
	.get_frame_desc = ti960_get_frame_desc,
	.enum_mbus_code = ti960_enum_mbus_code,
};

static struct v4l2_subdev_ops ti960_sd_ops = {
	.core = &ti960_core_subdev_ops,
	.video = &ti960_sd_video_ops,
	.pad = &ti960_sd_pad_ops,
};

static int ti960_register_subdev(struct ti960 *va)
{
	int i, ret;
	struct i2c_client *client = v4l2_get_subdevdata(&va->sd);
	u8 port = va->pdata->suffix - SUFFIX_BASE;
	u8 ctrl_num = ARRAY_SIZE(ti960_basic_controls) + 1;
	snprintf(va->sd.name, sizeof(va->sd.name), "TI960 %c",
		va->pdata->suffix);

	va->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	va->sd.internal_ops = &ti960_sd_internal_ops;
	va->sd.entity.function = MEDIA_ENT_F_VID_MUX;

	v4l2_set_subdevdata(&va->sd, client);

	ret = v4l2_ctrl_handler_init(&va->ctrl_handler, ctrl_num);

	if (ret < 0) {
		dev_err(va->sd.dev,
			"Failed to init ti960 controls. ERR: %d!\n",
			va->ctrl_handler.error);
		return va->ctrl_handler.error;
	}

	va->sd.ctrl_handler = &va->ctrl_handler;

	for (i = 0; i < ARRAY_SIZE(ti960_basic_controls); i++) {
		const struct v4l2_ctrl_config *cfg =
			&ti960_basic_controls[i];
		struct v4l2_ctrl *ctrl;

		ctrl = v4l2_ctrl_new_custom(&va->ctrl_handler, cfg, NULL);
		if (!ctrl) {
			dev_err(va->sd.dev,
				"Failed to create ctrl %s!\n", cfg->name);
			ret = va->ctrl_handler.error;
			goto failed_out;
		}
	}

	const struct v4l2_ctrl_config *cfg =
			&ti960_query_sub_control[port];
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_new_custom(&va->ctrl_handler, cfg, NULL);
	if (!ctrl) {
		dev_err(va->sd.dev,
			"Failed to create ctrl %s!\n", cfg->name);
		ret = va->ctrl_handler.error;
		goto failed_out;
	}

	va->link_freq = v4l2_ctrl_find(&va->ctrl_handler, V4L2_CID_LINK_FREQ);
	switch (va->pdata->link_freq_mbps) {
	case 1600:
		__v4l2_ctrl_s_ctrl(va->link_freq, TI960_MIPI_1600MBPS);
		break;
	case 1200:
		__v4l2_ctrl_s_ctrl(va->link_freq, TI960_MIPI_1200MBPS);
		break;
	case 800:
		__v4l2_ctrl_s_ctrl(va->link_freq, TI960_MIPI_800MBPS);
		break;
	case 400:
		__v4l2_ctrl_s_ctrl(va->link_freq, TI960_MIPI_400MBPS);
		break;
	default:
		break;
	}
	va->test_pattern = v4l2_ctrl_find(&va->ctrl_handler,
					  V4L2_CID_TEST_PATTERN);

	for (i = 0; i < va->nsinks; i++)
		va->pad[i].flags = MEDIA_PAD_FL_SINK;
	va->pad[TI960_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&va->sd.entity,
				      NR_OF_TI960_PADS, va->pad);
	if (ret) {
		dev_err(va->sd.dev,
			"Failed to init media entity for ti960!\n");
		goto failed_out;
	}

	return 0;

failed_out:
	v4l2_ctrl_handler_free(&va->ctrl_handler);
	return ret;
}

static int ti960_init(struct ti960 *va)
{
#ifdef TI960_RESET_NEEDED
	unsigned int reset_gpio = va->pdata->reset_gpio;
#endif
	int i, rval;
	unsigned int val;

#ifdef TI960_RESET_NEEDED
	/* TI960 PDB pulled up to high by HW design in some board */
	gpio_set_value(reset_gpio, 1);
	usleep_range(2000, 3000);
	dev_err(va->sd.dev, "Setting reset gpio %d to 1.\n", reset_gpio);
#endif

	bus_switch(va);
	usleep_range(8000, 9000);

	rval = ti960_reg_read(va, TI960_DEVID, &val);
	if (rval) {
		dev_err(va->sd.dev, "Failed to read device ID of TI960!\n");
		return rval;
	}
	dev_info(va->sd.dev, "TI960 device ID: 0x%X\n", val);
	for (i = 0; i < ARRAY_SIZE(ti960_gpio_settings); i++) {
		rval = ti960_reg_write(va,
			ti960_gpio_settings[i].reg,
			ti960_gpio_settings[i].val);
		if (rval) {
			dev_err(va->sd.dev,
				"Failed to write TI960 gpio setting, reg %2x, val %2x\n",
				ti960_gpio_settings[i].reg, ti960_gpio_settings[i].val);
			return rval;
		}
	}
	usleep_range(10000, 11000);

	for (i = 0; i < ARRAY_SIZE(ti960_init_settings); i++) {
		if (ti960_init_settings[i].reg == 0) {
			usleep_range(ti960_init_settings[i].val * 1000, ti960_init_settings[i].val * 1000);
			continue;
		}
		rval = ti960_reg_write(va,
			ti960_init_settings[i].reg,
			ti960_init_settings[i].val);
		if (rval) {
			dev_err(va->sd.dev,
				"Failed to write TI960 init setting, reg %2x, val %2x\n",
				ti960_init_settings[i].reg, ti960_init_settings[i].val);
			return rval;
		}
	}

	rval = ti960_map_subdevs_addr(va);
	if (rval)
		return rval;

	rval = ti960_set_frame_sync(va, 1);
	if (rval)
		dev_err(va->sd.dev,
					"Failed to write TI960 sync setting\n");
	return 0;
}

static void ti960_gpio_set(struct gpio_chip *chip, unsigned int gpio, int value)
{
	struct i2c_client *client = to_i2c_client(chip->parent);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ti960 *va = to_ti960(subdev);
	unsigned int reg_val;
	int rx_port, gpio_port;
	int ret;

	if (gpio >= NR_OF_TI960_GPIOS)
		return;

	rx_port = gpio / NR_OF_GPIOS_PER_PORT;
	gpio_port = gpio % NR_OF_GPIOS_PER_PORT;

	ret = ti960_reg_write(va, TI960_RX_PORT_SEL,
			  (rx_port << 4) + (1 << rx_port));
	if (ret) {
		dev_dbg(&client->dev, "Failed to select RX port.\n");
		return;
	}
	ret = regmap_read(va->regmap8, TI960_BC_GPIO_CTL0, &reg_val);
	if (ret) {
		dev_dbg(&client->dev, "Failed to read gpio status.\n");
		return;
	}

	if (gpio_port == 0) {
		reg_val &= ~TI960_GPIO0_MASK;
		reg_val |= value ? TI960_GPIO0_HIGH : TI960_GPIO0_LOW;
	} else {
		reg_val &= ~TI960_GPIO1_MASK;
		reg_val |= value ? TI960_GPIO1_HIGH : TI960_GPIO1_LOW;
	}

	ret = ti960_reg_write(va, TI960_BC_GPIO_CTL0, reg_val);
	if (ret)
		dev_dbg(&client->dev, "Failed to set gpio.\n");
}

static int ti960_gpio_direction_output(struct gpio_chip *chip,
				       unsigned int gpio, int level)
{
	return 0;
}

static int ti960_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct ti960 *va;
	int i, rval = 0;
	int gpio_FPD = 0;

	if (client->dev.platform_data == NULL)
		return -ENODEV;

	va = devm_kzalloc(&client->dev, sizeof(*va), GFP_KERNEL);
	if (!va)
		return -ENOMEM;

	va->pdata = client->dev.platform_data;

	va->nsources = NR_OF_TI960_SOURCE_PADS;
	va->nsinks = NR_OF_TI960_SINK_PADS;
	va->npads = NR_OF_TI960_PADS;
	va->nstreams = NR_OF_TI960_STREAMS;

	va->crop = devm_kcalloc(&client->dev, va->npads,
				sizeof(struct v4l2_rect), GFP_KERNEL);

	va->compose = devm_kcalloc(&client->dev, va->npads,
				   sizeof(struct v4l2_rect), GFP_KERNEL);

	if (!va->crop || !va->compose)
		return -ENOMEM;

	for (i = 0; i < va->npads; i++) {
		va->ffmts[i] = devm_kcalloc(&client->dev, va->nstreams,
					    sizeof(struct v4l2_mbus_framefmt),
					    GFP_KERNEL);
		if (!va->ffmts[i])
			return -ENOMEM;
	}

	va->regmap8 = devm_regmap_init_i2c(client,
					   &ti960_reg_config8);
	if (IS_ERR(va->regmap8)) {
		dev_err(&client->dev, "Failed to init regmap8!\n");
		return -EIO;
	}

	va->regmap16 = devm_regmap_init_i2c(client,
					    &ti960_reg_config16);
	if (IS_ERR(va->regmap16)) {
		dev_err(&client->dev, "Failed to init regmap16!\n");
		return -EIO;
	}

	mutex_init(&va->mutex);
	v4l2_i2c_subdev_init(&va->sd, client, &ti960_sd_ops);
	rval = ti960_register_subdev(va);
	if (rval) {
		dev_err(&client->dev, "Failed to register va subdevice!\n");
		return rval;
	}

#ifdef TI960_RESET_NEEDED
	if (devm_gpio_request_one(va->sd.dev, va->pdata->reset_gpio, 0,
				  "ti960 reset") != 0) {
		dev_err(va->sd.dev, "Unable to acquire gpio %d\n",
			va->pdata->reset_gpio);
		return -ENODEV;
	}
#endif

	if (va->pdata->FPD_gpio != -1) {
		rval = devm_gpio_request_one(&client->dev,
			va->pdata->FPD_gpio,
			GPIOF_OUT_INIT_LOW, "Cam");
		if (rval) {
			dev_err(&client->dev,
				"camera power GPIO pin request failed!\n");
			return rval;
		}

		/* pull up GPPC_B23 to high for FPD link power */
		gpio_FPD = gpio_get_value(va->pdata->FPD_gpio);
		if (gpio_FPD == 0)
			gpio_set_value(va->pdata->FPD_gpio, 1);
	}

	va->bus_switch = i2c_new_dummy_device(client->adapter, D3_BUS_SWITCH_ADDR);
	if (IS_ERR(va->bus_switch)) {
		dev_warn(&client->dev, "Failed to create client for bus-switch: %ld",
				PTR_ERR(va->bus_switch));
		va->bus_switch = 0;
	}
	rval = ti960_init(va);
	if (rval) {
		dev_err(&client->dev, "Failed to init TI960!\n");
		goto free_gpio;
	}

	/*
	 * TI960 has several back channel GPIOs.
	 * We export GPIO0 and GPIO1 to control reset or fsin.
	 */
	va->gc.parent = &client->dev;
	va->gc.owner = THIS_MODULE;
	va->gc.label = "TI960 GPIO";
	va->gc.ngpio = NR_OF_TI960_GPIOS;
	va->gc.base = -1;
	va->gc.set = ti960_gpio_set;
	va->gc.direction_output = ti960_gpio_direction_output;
	rval = gpiochip_add(&va->gc);
	if (rval) {
		dev_err(&client->dev, "Failed to add gpio chip! %d\n", rval);
		rval = -EIO;
		goto free_gpio;
	}

	dev_err(&client->dev, "%s Probe Succeeded", va->sd.name);
	dev_err(&client->dev, "%s Link Freq %d Mbps", va->sd.name, va->pdata->link_freq_mbps);
	return 0;

free_gpio:
	if (va->pdata->FPD_gpio != -1) {
		dev_err(&client->dev, "restore and free FPD gpio!\n");
		/* restore GPPC_B23 */
		if (gpio_FPD == 0)
			gpio_set_value(va->pdata->FPD_gpio, 0);

		gpio_free(va->pdata->FPD_gpio);
	}

	dev_err(&client->dev, "%s Probe Failed", va->sd.name);
	return rval;
}

static void ti960_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ti960 *va = to_ti960(subdev);
	int i;

	if (!va)
		return;

	mutex_destroy(&va->mutex);
	v4l2_ctrl_handler_free(&va->ctrl_handler);
	v4l2_device_unregister_subdev(&va->sd);
	media_entity_cleanup(&va->sd.entity);

	for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
		if (va->sub_devs[i].sd) {
			struct i2c_client *sub_client =
				v4l2_get_subdevdata(va->sub_devs[i].sd);

			i2c_unregister_device(sub_client);
		}
		va->sub_devs[i].sd = NULL;
		if (va->sub_devs[i].serializer) {
			i2c_unregister_device(va->sub_devs[i].serializer);
			va->sub_devs[i].serializer = NULL;
		}

		if (va->sub_devs[i].gpio_exp) {
			i2c_unregister_device(va->sub_devs[i].gpio_exp);
			va->sub_devs[i].gpio_exp = NULL;
		}
	}

	if (va->bus_switch) {
		i2c_unregister_device(va->bus_switch);
		va->bus_switch = NULL;
	}

	gpiochip_remove(&va->gc);

}

#ifdef CONFIG_PM
static int ti960_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ti960 *va = to_ti960(subdev);
	int i;
	int rval;

	for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
		if (va->sub_devs[i].serializer) {
			i2c_unregister_device(va->sub_devs[i].serializer);
			va->sub_devs[i].serializer = NULL;
		}

		if (va->sub_devs[i].gpio_exp) {
			i2c_unregister_device(va->sub_devs[i].gpio_exp);
			va->sub_devs[i].gpio_exp = NULL;
		}

		rval = ti960_unmap_i2c_slave(va, &va->sub_devs[i],
					D3_GPIO_EXP_ADDR);
		if (rval)
			return rval;
	}

	rval = ti960_unmap_subdevs_addr(va);
	if (rval)
		return rval;

	return 0;
}

static int ti960_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ti960 *va = to_ti960(subdev);
	int i, rval;
	struct ti960_subdev *sensor_subdev;
	struct ti960_subdev_pdata *pdata;

	rval = ti960_init(va);
	if (rval) {
		dev_err(va->sd.dev, "resume: failed init ti960");
		return rval;
	}

	for (i = 0; i < NR_OF_TI960_SINK_PADS; i++) {
		sensor_subdev = &va->sub_devs[i];
		pdata = &va->subdev_pdata[i];
		if (sensor_subdev->sd == NULL)
			break;

		rval = ti960_config_ser(va, client, i, sensor_subdev, pdata);
		if (rval)
			dev_warn(va->sd.dev, "resume: failed config subdev");

		rval = reset_sensor(va, &va->sub_devs[i], &va->subdev_pdata[i]);
		if (rval) {
			dev_warn(va->sd.dev, "sensor failed to reset.\n");
			continue;
		}
	}

	return 0;
}
#else
#define ti960_suspend	NULL
#define ti960_resume	NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id ti960_id_table[] = {
	{ TI960_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ti960_id_table);

static const struct dev_pm_ops ti960_pm_ops = {
	.suspend = ti960_suspend,
	.resume = ti960_resume,
};

static struct i2c_driver ti960_i2c_driver = {
	.driver = {
		.name = TI960_NAME,
		.pm = &ti960_pm_ops,
	},
	.probe = ti960_probe,
	.remove	= ti960_remove,
	.id_table = ti960_id_table,
};
module_i2c_driver(ti960_i2c_driver);

MODULE_AUTHOR("Chen Meng J <meng.j.chen@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TI960 CSI2-Aggregator driver");
