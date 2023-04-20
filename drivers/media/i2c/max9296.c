/*
 * max9296.c - max9296 GMSL Deserializer driver
 *
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
// #include <media/camera_common.h>
#include <linux/regmap.h>
#include <media/max9296.h>

/* register specifics */
#define MAX9296_DST_CSI_MODE_ADDR 0x330
#define MAX9296_LANE_MAP1_ADDR 0x333
#define MAX9296_LANE_MAP2_ADDR 0x334

#define MAX9296_LANE_CTRL0_ADDR 0x40A
#define MAX9296_LANE_CTRL1_ADDR 0x44A
#define MAX9296_LANE_CTRL2_ADDR 0x48A
#define MAX9296_LANE_CTRL3_ADDR 0x4CA

#define MAX9296_TX11_PIPE_X_EN_ADDR 0x40B
#define MAX9296_TX45_PIPE_X_DST_CTRL_ADDR 0x42D

#define MAX9296_PIPE_X_SRC_0_MAP_ADDR 0x40D
#define MAX9296_PIPE_X_DST_0_MAP_ADDR 0x40E
#define MAX9296_PIPE_X_SRC_1_MAP_ADDR 0x40F
#define MAX9296_PIPE_X_DST_1_MAP_ADDR 0x410
#define MAX9296_PIPE_X_SRC_2_MAP_ADDR 0x411
#define MAX9296_PIPE_X_DST_2_MAP_ADDR 0x412
#define MAX9296_PIPE_X_SRC_3_MAP_ADDR 0x413
#define MAX9296_PIPE_X_DST_3_MAP_ADDR 0x414

#define MAX9296_PIPE_X_ST_SEL_ADDR 0x50

#define MAX9296_PWDN_PHYS_ADDR 0x332
#define MAX9296_PHY1_CLK_ADDR 0x320
#define MAX9296_CTRL0_ADDR 0x10

/* data defines */
#define MAX9296_CSI_MODE_4X2 0x1
#define MAX9296_CSI_MODE_2X4 0x4
#define MAX9296_LANE_MAP1_4X2 0x44
#define MAX9296_LANE_MAP2_4X2 0x44
#define MAX9296_LANE_MAP1_2X4 0x4E
#define MAX9296_LANE_MAP2_2X4 0xE4

#define MAX9296_LANE_CTRL_MAP(num_lanes) \
	(((num_lanes) << 6) & 0xF0)

#define MAX9296_ALLPHYS_NOSTDBY 0xF0
#define MAX9296_ST_ID_SEL_INVALID 0xF

#define MAX9296_PHY1_CLK 0x2C

#define MAX9296_RESET_ALL 0x80

/* Dual GMSL MAX9296A/B */
#define MAX9296_MAX_SOURCES 2

#define MAX9296_MAX_PIPES 4

#define MAX9296_PIPE_X 0
#define MAX9296_PIPE_Y 1
#define MAX9296_PIPE_Z 2
#define MAX9296_PIPE_U 3
#define MAX9296_PIPE_INVALID 0xF


#define MAX9296_CSI_CTRL_0 0
#define MAX9296_CSI_CTRL_1 1
#define MAX9296_CSI_CTRL_2 2
#define MAX9296_CSI_CTRL_3 3

#define MAX9296_INVAL_ST_ID 0xFF

/* Use reset value as per spec, confirm with vendor */
#define MAX9296_RESET_ST_ID 0x00

struct max9296_source_ctx {
	struct gmsl_link_ctx *g_ctx;
	bool st_enabled;
};

struct pipe_ctx {
	u32 id;
	u32 dt_type;
	u32 dst_csi_ctrl;
	u32 st_count;
	u32 st_id_sel;
};

struct max9296 {
	struct i2c_client *i2c_client;
	struct regmap *regmap;
	u32 num_src;
	u32 max_src;
	u32 num_src_found;
	u32 src_link;
	bool splitter_enabled;
	struct max9296_source_ctx sources[MAX9296_MAX_SOURCES];
	struct mutex lock;
	u32 sdev_ref;
	bool lane_setup;
	bool link_setup;
	struct pipe_ctx pipe[MAX9296_MAX_PIPES];
	u8 csi_mode;
	u8 lane_mp1;
	u8 lane_mp2;
	int reset_gpio;
	int pw_ref;
	struct regulator *vdd_cam_1v2;
};

static int max9296_write_reg(struct device *dev,
	u16 addr, u8 val)
{
	struct max9296 *priv;
	int err;

	priv = dev_get_drvdata(dev);

	err = regmap_write(priv->regmap, addr, val);
	if (err)
		dev_err(dev,
		"%s:i2c write failed, 0x%x = %x\n",
		__func__, addr, val);

	/* delay before next i2c command as required for SERDES link */
	usleep_range(100, 110);

	return err;
}

static int max9296_get_sdev_idx(struct device *dev,
			struct device *s_dev, unsigned int *idx)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	unsigned int i;
	int err = 0;

	mutex_lock(&priv->lock);
	for (i = 0; i < priv->max_src; i++) {
		if (priv->sources[i].g_ctx->s_dev == s_dev)
			break;
	}
	if (i == priv->max_src) {
		dev_err(dev, "no sdev found\n");
		err = -EINVAL;
		goto ret;
	}

	if (idx)
		*idx = i;

ret:
	mutex_unlock(&priv->lock);
	return err;
}

static void max9296_pipes_reset(struct max9296 *priv)
{
	/*
	 * This is default pipes combination. add more mappings
	 * for other combinations and requirements.
	 */
	struct pipe_ctx pipe_defaults[] = {
		{MAX9296_PIPE_X, GMSL_CSI_DT_RAW_12,
			MAX9296_CSI_CTRL_1, 0, MAX9296_INVAL_ST_ID},
		{MAX9296_PIPE_Y, GMSL_CSI_DT_RAW_12,
			MAX9296_CSI_CTRL_1, 0, MAX9296_INVAL_ST_ID},
		{MAX9296_PIPE_Z, GMSL_CSI_DT_EMBED,
			MAX9296_CSI_CTRL_1, 0, MAX9296_INVAL_ST_ID},
		{MAX9296_PIPE_U, GMSL_CSI_DT_EMBED,
			MAX9296_CSI_CTRL_1, 0, MAX9296_INVAL_ST_ID}
	};

	/*
	 * Add DT props for num-streams and stream sequence, and based on that
	 * set the appropriate pipes defaults.
	 * For now default it supports "2 RAW12 and 2 EMBED" 1:1 mappings.
	 */
	memcpy(priv->pipe, pipe_defaults, sizeof(pipe_defaults));
}

static void max9296_reset_ctx(struct max9296 *priv)
{
	unsigned int i;

	priv->link_setup = false;
	priv->lane_setup = false;
	priv->num_src_found = 0;
	priv->src_link = 0;
	priv->splitter_enabled = false;
	max9296_pipes_reset(priv);
	for (i = 0; i < priv->num_src; i++)
		priv->sources[i].st_enabled = false;
}

int max9296_power_on(struct device *dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	int err = 0;

	mutex_lock(&priv->lock);
	if (priv->pw_ref == 0) {
		usleep_range(1, 2);
		if (priv->reset_gpio)
			gpio_set_value(priv->reset_gpio, 0);

		usleep_range(30, 50);

		if (priv->vdd_cam_1v2) {
			err = regulator_enable(priv->vdd_cam_1v2);
			if (unlikely(err))
				goto ret;
		}

		usleep_range(30, 50);

		/*exit reset mode: XCLR */
		if (priv->reset_gpio) {
			gpio_set_value(priv->reset_gpio, 0);
			usleep_range(30, 50);
			gpio_set_value(priv->reset_gpio, 1);
			usleep_range(30, 50);
		}

		/* delay to settle reset */
		msleep(20);
	}

	priv->pw_ref++;

ret:
	mutex_unlock(&priv->lock);

	return err;
}
EXPORT_SYMBOL(max9296_power_on);

void max9296_power_off(struct device *dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);

	mutex_lock(&priv->lock);
	priv->pw_ref--;

	if (priv->pw_ref < 0)
		priv->pw_ref = 0;

	if (priv->pw_ref == 0) {
		/* enter reset mode: XCLR */
		usleep_range(1, 2);
		if (priv->reset_gpio)
			gpio_set_value(priv->reset_gpio, 0);

		if (priv->vdd_cam_1v2)
			regulator_disable(priv->vdd_cam_1v2);
	}

	mutex_unlock(&priv->lock);
}
EXPORT_SYMBOL(max9296_power_off);

static int max9296_write_link(struct device *dev, u32 link)
{
	if (link == GMSL_SERDES_CSI_LINK_A) {
		max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x01);
		max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x21);
	} else if (link == GMSL_SERDES_CSI_LINK_B) {
		max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x02);
		max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x22);
	} else {
		dev_err(dev, "%s: invalid gmsl link\n", __func__);
		return -EINVAL;
	}

	/* delay to settle link */
	msleep(100);

	return 0;
}

int max9296_setup_link(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	int err = 0;
	unsigned int i = 0;

	err = max9296_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	mutex_lock(&priv->lock);

	if (!priv->splitter_enabled) {
		err = max9296_write_link(dev,
				priv->sources[i].g_ctx->serdes_csi_link);
		if (err)
			goto ret;

		priv->link_setup = true;
	}

ret:
	mutex_unlock(&priv->lock);

	return err;
}
EXPORT_SYMBOL(max9296_setup_link);

int max9296_setup_control(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	int err = 0;
	unsigned int i = 0;

	err = max9296_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	mutex_lock(&priv->lock);

	if (!priv->link_setup) {
		dev_err(dev, "%s: invalid state\n", __func__);
		err = -EINVAL;
		goto error;
	}

	if (priv->sources[i].g_ctx->serdev_found) {
		priv->num_src_found++;
		priv->src_link = priv->sources[i].g_ctx->serdes_csi_link;
	}

	/* Enable splitter mode */
	if ((priv->max_src > 1U) &&
		(priv->num_src_found > 0U) &&
		(priv->splitter_enabled == false)) {
		max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x03);
		max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x23);

		priv->splitter_enabled = true;

		/* delay to settle link */
		msleep(100);
	}

	max9296_write_reg(dev,
			MAX9296_PWDN_PHYS_ADDR, MAX9296_ALLPHYS_NOSTDBY);

	priv->sdev_ref++;

	/* Reset splitter mode if all devices are not found */
	if ((priv->sdev_ref != priv->max_src) &&
		(priv->splitter_enabled == true) &&
		(priv->num_src_found > 0U) &&
		(priv->num_src_found < priv->max_src)) {
		err = max9296_write_link(dev, priv->src_link);
		if (err)
			goto error;

		priv->splitter_enabled = false;
	}

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max9296_setup_control);

int max9296_reset_control(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	int err = 0;

	mutex_lock(&priv->lock);
	if (!priv->sdev_ref) {
		dev_info(dev, "%s: dev is already in reset state\n", __func__);
		goto ret;
	}

	priv->sdev_ref--;
	if (priv->sdev_ref == 0) {
		max9296_reset_ctx(priv);
		max9296_write_reg(dev, MAX9296_CTRL0_ADDR, MAX9296_RESET_ALL);

		/* delay to settle reset */
		msleep(100);
	}

ret:
	mutex_unlock(&priv->lock);

	return err;
}
EXPORT_SYMBOL(max9296_reset_control);

int max9296_sdev_register(struct device *dev, struct gmsl_link_ctx *g_ctx)
{
	struct max9296 *priv = NULL;
	unsigned int i;
	int err = 0;

	if (!dev || !g_ctx || !g_ctx->s_dev) {
		dev_err(dev, "%s: invalid input params\n", __func__);
		return -EINVAL;
	}

	priv = dev_get_drvdata(dev);

	mutex_lock(&priv->lock);

	if (priv->num_src > priv->max_src) {
		dev_err(dev,
			"%s: MAX9296 inputs size exhausted\n", __func__);
		err = -ENOMEM;
		goto error;
	}

	/* Check csi mode compatibility */
	if (!((priv->csi_mode == MAX9296_CSI_MODE_2X4) ?
			((g_ctx->csi_mode == GMSL_CSI_1X4_MODE) ||
				(g_ctx->csi_mode == GMSL_CSI_2X4_MODE)) :
			((g_ctx->csi_mode == GMSL_CSI_2X2_MODE) ||
				(g_ctx->csi_mode == GMSL_CSI_4X2_MODE)))) {
		dev_err(dev,
			"%s: csi mode not supported\n", __func__);
		err = -EINVAL;
		goto error;
	}

	for (i = 0; i < priv->num_src; i++) {
		if (g_ctx->serdes_csi_link ==
			priv->sources[i].g_ctx->serdes_csi_link) {
			dev_err(dev,
				"%s: serdes csi link is in use\n", __func__);
			err = -EINVAL;
			goto error;
		}
		/*
		 * All sdevs should have same num-csi-lanes regardless of
		 * dst csi port selected.
		 * Later if there is any usecase which requires each port
		 * to be configured with different num-csi-lanes, then this
		 * check should be performed per port.
		 */
		if (g_ctx->num_csi_lanes !=
				priv->sources[i].g_ctx->num_csi_lanes) {
			dev_err(dev,
				"%s: csi num lanes mismatch\n", __func__);
			err = -EINVAL;
			goto error;
		}
	}

	priv->sources[priv->num_src].g_ctx = g_ctx;
	priv->sources[priv->num_src].st_enabled = false;

	priv->num_src++;

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max9296_sdev_register);

int max9296_sdev_unregister(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = NULL;
	int err = 0;
	unsigned int i = 0;

	if (!dev || !s_dev) {
		dev_err(dev, "%s: invalid input params\n", __func__);
		return -EINVAL;
	}

	priv = dev_get_drvdata(dev);
	mutex_lock(&priv->lock);

	if (priv->num_src == 0) {
		dev_err(dev, "%s: no source found\n", __func__);
		err = -ENODATA;
		goto error;
	}

	for (i = 0; i < priv->num_src; i++) {
		if (s_dev == priv->sources[i].g_ctx->s_dev) {
			priv->sources[i].g_ctx = NULL;
			break;
		}
	}

	if (i == priv->num_src) {
		dev_err(dev,
			"%s: requested device not found\n", __func__);
		err = -EINVAL;
		goto error;
	}
	priv->num_src--;

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max9296_sdev_unregister);

static int max9296_get_available_pipe(struct device *dev,
				u32 st_data_type, u32 dst_csi_port)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < MAX9296_MAX_PIPES; i++) {
		/*
		 * TODO: Enable a pipe for multi stream configuration having
		 * similar stream data type. For now use st_count as a flag
		 * for 1 to 1 mapping in pipe and stream data type, same can
		 * be extended as count for many to 1 mapping. Would also need
		 * few more checks such as input stream id select, dst port etc.
		 */
		if ((priv->pipe[i].dt_type == st_data_type) &&
			((dst_csi_port == GMSL_CSI_PORT_A) ?
				(priv->pipe[i].dst_csi_ctrl ==
					MAX9296_CSI_CTRL_0) ||
				(priv->pipe[i].dst_csi_ctrl ==
					MAX9296_CSI_CTRL_1) :
				(priv->pipe[i].dst_csi_ctrl ==
					MAX9296_CSI_CTRL_2) ||
				(priv->pipe[i].dst_csi_ctrl ==
					MAX9296_CSI_CTRL_3)) &&
			(!priv->pipe[i].st_count))
			break;
	}

	if (i == MAX9296_MAX_PIPES) {
		dev_err(dev, "%s: all pipes are busy\n", __func__);
		return -ENOMEM;
	}

	return i;
}

struct reg_pair {
	u16 addr;
	u8 val;
};

static int max9296_setup_pipeline(struct device *dev,
		struct gmsl_link_ctx *g_ctx)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	struct gmsl_stream *g_stream;
	struct reg_pair *map_list;
	u32 arr_sz = 0;
	int pipe_id = 0;
	u32 i = 0;
	u32 j = 0;
	u32 vc_idx = 0;

	for (i = 0; i < g_ctx->num_streams; i++) {
		/* Base data type mapping: pipeX/RAW12/CSICNTR1 */
		struct reg_pair map_pipe_raw12[] = {
			/* addr, val */
			{MAX9296_TX11_PIPE_X_EN_ADDR, 0x7},
			{MAX9296_TX45_PIPE_X_DST_CTRL_ADDR, 0x15},
			{MAX9296_PIPE_X_SRC_0_MAP_ADDR, 0x2C},
			{MAX9296_PIPE_X_DST_0_MAP_ADDR, 0x2C},
			{MAX9296_PIPE_X_SRC_1_MAP_ADDR, 0x00},
			{MAX9296_PIPE_X_DST_1_MAP_ADDR, 0x00},
			{MAX9296_PIPE_X_SRC_2_MAP_ADDR, 0x01},
			{MAX9296_PIPE_X_DST_2_MAP_ADDR, 0x01},
		};

		/* Base data type mapping: pipeX/EMBED/CSICNTR1 */
		struct reg_pair map_pipe_embed[] = {
			/* addr, val */
			{MAX9296_TX11_PIPE_X_EN_ADDR, 0x7},
			{MAX9296_TX45_PIPE_X_DST_CTRL_ADDR, 0x15},
			{MAX9296_PIPE_X_SRC_0_MAP_ADDR, 0x12},
			{MAX9296_PIPE_X_DST_0_MAP_ADDR, 0x12},
			{MAX9296_PIPE_X_SRC_1_MAP_ADDR, 0x00},
			{MAX9296_PIPE_X_DST_1_MAP_ADDR, 0x00},
			{MAX9296_PIPE_X_SRC_2_MAP_ADDR, 0x01},
			{MAX9296_PIPE_X_DST_2_MAP_ADDR, 0x01},
		};

		g_stream = &g_ctx->streams[i];
		g_stream->des_pipe = MAX9296_PIPE_INVALID;

		if (g_stream->st_data_type == GMSL_CSI_DT_RAW_12) {
			map_list = map_pipe_raw12;
			arr_sz = ARRAY_SIZE(map_pipe_raw12);
		} else if (g_stream->st_data_type == GMSL_CSI_DT_EMBED) {
			map_list = map_pipe_embed;
			arr_sz = ARRAY_SIZE(map_pipe_embed);
		} else if (g_stream->st_data_type == GMSL_CSI_DT_UED_U1) {
			dev_dbg(dev,
				"%s: No mapping for GMSL_CSI_DT_UED_U1\n",
				__func__);
			continue;
		} else {
			dev_err(dev, "%s: Invalid data type\n", __func__);
			return -EINVAL;
		}

		pipe_id = max9296_get_available_pipe(dev,
				g_stream->st_data_type, g_ctx->dst_csi_port);
		if (pipe_id < 0)
			return pipe_id;

		for (j = 0, vc_idx = 3; j < arr_sz; j++, vc_idx += 2) {
			/* update pipe configuration */
			map_list[j].addr += (0x40 * pipe_id);
			/* update vc id configuration */
			if (vc_idx < arr_sz)
				map_list[vc_idx].val |=
					(g_ctx->dst_vc << 6);

			max9296_write_reg(dev, map_list[j].addr,
						map_list[j].val);
		}

		/* Set stream id select input */
		if (g_stream->st_id_sel == GMSL_ST_ID_UNUSED) {
			dev_err(dev, "%s: Invalid stream st_id_sel\n",
				__func__);
			return -EINVAL;
		}

		g_stream->des_pipe = MAX9296_PIPE_X_ST_SEL_ADDR + pipe_id;

		/* Update pipe internals */
		priv->pipe[pipe_id].st_count++;
		priv->pipe[pipe_id].st_id_sel = g_stream->st_id_sel;
	}

	return 0;
}

int max9296_start_streaming(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	struct gmsl_link_ctx *g_ctx;
	struct gmsl_stream *g_stream;
	int err = 0;
	unsigned int i = 0;

	err = max9296_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	mutex_lock(&priv->lock);
	g_ctx = priv->sources[i].g_ctx;

	for (i = 0; i < g_ctx->num_streams; i++) {
		g_stream = &g_ctx->streams[i];

		if (g_stream->des_pipe != MAX9296_PIPE_INVALID)
			max9296_write_reg(dev, g_stream->des_pipe,
						g_stream->st_id_sel);
	}
	mutex_unlock(&priv->lock);

	return 0;
}
EXPORT_SYMBOL(max9296_start_streaming);

int max9296_stop_streaming(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	struct gmsl_link_ctx *g_ctx;
	struct gmsl_stream *g_stream;
	int err = 0;
	unsigned int i = 0;

	err = max9296_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	mutex_lock(&priv->lock);
	g_ctx = priv->sources[i].g_ctx;

	for (i = 0; i < g_ctx->num_streams; i++) {
		g_stream = &g_ctx->streams[i];

		if (g_stream->des_pipe != MAX9296_PIPE_INVALID)
			max9296_write_reg(dev, g_stream->des_pipe,
						MAX9296_RESET_ST_ID);
	}

	mutex_unlock(&priv->lock);

	return 0;
}
EXPORT_SYMBOL(max9296_stop_streaming);

int max9296_setup_streaming(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	struct gmsl_link_ctx *g_ctx;
	int err = 0;
	unsigned int i = 0;
	u16 lane_ctrl_addr;

	err = max9296_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	mutex_lock(&priv->lock);
	if (priv->sources[i].st_enabled)
		goto ret;

	g_ctx = priv->sources[i].g_ctx;

	err = max9296_setup_pipeline(dev, g_ctx);
	if (err)
		goto ret;

	/* Derive CSI lane map register */
	switch(g_ctx->dst_csi_port) {
	case GMSL_CSI_PORT_A:
	case GMSL_CSI_PORT_D:
		lane_ctrl_addr = MAX9296_LANE_CTRL1_ADDR;
		break;
	case GMSL_CSI_PORT_B:
	case GMSL_CSI_PORT_E:
		lane_ctrl_addr = MAX9296_LANE_CTRL2_ADDR;
		break;
	case GMSL_CSI_PORT_C:
		lane_ctrl_addr = MAX9296_LANE_CTRL0_ADDR;
		break;
	case GMSL_CSI_PORT_F:
		lane_ctrl_addr = MAX9296_LANE_CTRL3_ADDR;
		break;
	default:
		dev_err(dev, "%s: invalid gmsl csi port!\n", __func__);
		err = -EINVAL;
		goto ret;
	};

	/*
	 * rewrite num_lanes to same dst port should not be an issue,
	 * as the device compatibility is already
	 * checked during sdev registration against the des properties.
	 */
	max9296_write_reg(dev, lane_ctrl_addr,
		MAX9296_LANE_CTRL_MAP(g_ctx->num_csi_lanes-1));

	if (!priv->lane_setup) {
		max9296_write_reg(dev,
			MAX9296_DST_CSI_MODE_ADDR, priv->csi_mode);
		max9296_write_reg(dev,
			MAX9296_LANE_MAP1_ADDR, priv->lane_mp1);
		max9296_write_reg(dev,
			MAX9296_LANE_MAP2_ADDR, priv->lane_mp2);
		max9296_write_reg(dev,
			MAX9296_PHY1_CLK_ADDR, MAX9296_PHY1_CLK);

		priv->lane_setup = true;
	}

	priv->sources[i].st_enabled = true;

ret:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max9296_setup_streaming);

static int max9296_set_registers(struct device *dev, struct reg_pair *map,
				 u32 count)
{
	int err = 0;
	u32 j = 0;

	for (j = 0; j < count; j++) {
		err = max9296_write_reg(dev,
			map[j].addr, map[j].val);
		if (err != 0)
			break;
	}

	return err;
}

int max9296_get_available_pipe_id(struct device *dev, int vc_id)
{
	int i;
	int pipe_id = -ENOMEM;
	struct max9296 *priv = dev_get_drvdata(dev);

	mutex_lock(&priv->lock);
	for (i = 0; i < MAX9296_MAX_PIPES; i++) {
		if (i == vc_id && !priv->pipe[i].st_count) {
			priv->pipe[i].st_count++;
			pipe_id = i;
			break;
		}
	}
	mutex_unlock(&priv->lock);

	return pipe_id;
}
EXPORT_SYMBOL(max9296_get_available_pipe_id);

int max9296_release_pipe(struct device *dev, int pipe_id)
{
	struct max9296 *priv = dev_get_drvdata(dev);

	if (pipe_id < 0 || pipe_id >= MAX9296_MAX_PIPES)
		return -EINVAL;

	mutex_lock(&priv->lock);
	priv->pipe[pipe_id].st_count = 0;
	mutex_unlock(&priv->lock);

	return 0;
}
EXPORT_SYMBOL(max9296_release_pipe);

void max9296_reset_oneshot(struct device *dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);

	mutex_lock(&priv->lock);
	if (priv->splitter_enabled) {
		max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x03);
		max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x23);
	} else {
		max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x31);
	}
	/* delay to settle link */
	msleep(100);
	mutex_unlock(&priv->lock);
}
EXPORT_SYMBOL(max9296_reset_oneshot);

static int __max9296_set_pipe(struct device *dev, int pipe_id, u8 data_type1,
			      u8 data_type2, u32 vc_id)
{
	int err = 0;
	int i = 0;
	u8 en_mapping_num = 0x0F;
	u8 all_mapping_phy = 0x55;
	struct reg_pair map_pipe_opt[] = {
		{0x1458, 0x28}, // PHY A Optimization
		{0x1459, 0x68}, // PHY A Optimization
		{0x1558, 0x28}, // PHY B Optimization
		{0x1559, 0x68}, // PHY B Optimization
		// 4 lanes on port A, write 0x50 for 2 lanes
		{MAX9296_LANE_CTRL1_ADDR, 0x50},
		// 1500Mbps/lane on port A
		{MAX9296_PHY1_CLK_ADDR, 0x2F},
		// Do not un-double 8bpp (Un-double 8bpp data)
		//{0x031C, 0x00},
		// Do not un-double 8bpp
		//{0x031F, 0x00},
		// 0x02: ALT_MEM_MAP8, 0x10: ALT2_MEM_MAP8
		{0x0473, 0x10},
	};
	struct reg_pair map_pipe_control[] = {
		// Enable 4 mappings for Pipe X
		{MAX9296_TX11_PIPE_X_EN_ADDR, 0x0F},
		// Map data_type1 on vc_id
		{MAX9296_PIPE_X_SRC_0_MAP_ADDR, 0x1E},
		{MAX9296_PIPE_X_DST_0_MAP_ADDR, 0x1E},
		// Map frame_start on vc_id
		{MAX9296_PIPE_X_SRC_1_MAP_ADDR, 0x00},
		{MAX9296_PIPE_X_DST_1_MAP_ADDR, 0x00},
		// Map frame end on vc_id
		{MAX9296_PIPE_X_SRC_2_MAP_ADDR, 0x01},
		{MAX9296_PIPE_X_DST_2_MAP_ADDR, 0x01},
		// Map data_type2 on vc_id
		{MAX9296_PIPE_X_SRC_3_MAP_ADDR, 0x12},
		{MAX9296_PIPE_X_DST_3_MAP_ADDR, 0x12},
		// All mappings to PHY1 (master for port A)
		{MAX9296_TX45_PIPE_X_DST_CTRL_ADDR, 0x55},
		// SEQ_MISS_EN: Disabled / DIS_PKT_DET: Disabled
		{0x0100, 0x23}, // pipe X
	};

	for (i = 0; i < 10; i++) {
		map_pipe_control[i].addr += 0x40 * pipe_id;
	}
	map_pipe_control[10].addr += 0x12 * pipe_id;

	if (data_type2 == 0x0) {
		en_mapping_num = 0x07;
		all_mapping_phy = 0x15;
	}
	map_pipe_control[0].val = en_mapping_num;
	map_pipe_control[1].val = (vc_id << 6) | data_type1;
	map_pipe_control[2].val = (vc_id << 6) | data_type1;
	map_pipe_control[3].val = (vc_id << 6) | 0x00;
	map_pipe_control[4].val = (vc_id << 6) | 0x00;
	map_pipe_control[5].val = (vc_id << 6) | 0x01;
	map_pipe_control[6].val = (vc_id << 6) | 0x01;
	map_pipe_control[7].val = (vc_id << 6) | data_type2;
	map_pipe_control[8].val = (vc_id << 6) | data_type2;
	map_pipe_control[9].val = all_mapping_phy;
	map_pipe_control[10].val = 0x23;

	err |= max9296_set_registers(dev, map_pipe_control,
				     ARRAY_SIZE(map_pipe_control));

	err |= max9296_set_registers(dev, map_pipe_opt,
				     ARRAY_SIZE(map_pipe_opt));

	return err;
}
int max9296_init_settings(struct device *dev)
{
	int err = 0;
	int i;
	struct max9296 *priv = dev_get_drvdata(dev);

	mutex_lock(&priv->lock);

	for (i = 0; i < MAX9296_MAX_PIPES; i++)
		err |= __max9296_set_pipe(dev, i, GMSL_CSI_DT_YUV422_8,
					  GMSL_CSI_DT_EMBED, i);

	mutex_unlock(&priv->lock);

	return err;
}
EXPORT_SYMBOL(max9296_init_settings);

int max9296_set_pipe(struct device *dev, int pipe_id,
		     u8 data_type1, u8 data_type2, u32 vc_id)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	int err = 0;

	if (pipe_id > (MAX9296_MAX_PIPES - 1)) {
		dev_info(dev, "%s, input pipe_id: %d exceed max9296 max pipes\n",
			 __func__, pipe_id);
		return -EINVAL;
	}

	dev_dbg(dev, "%s pipe_id %d, data_type1 %u, data_type2 %u, vc_id %u\n",
		__func__, pipe_id, data_type1, data_type2, vc_id);

	mutex_lock(&priv->lock);

	err = __max9296_set_pipe(dev, pipe_id, data_type1, data_type2, vc_id);

	mutex_unlock(&priv->lock);

	return err;
}
EXPORT_SYMBOL(max9296_set_pipe);

static const struct of_device_id max9296_of_match[] = {
	{ .compatible = "maxim,max9296", },
	{ },
};
MODULE_DEVICE_TABLE(of, max9296_of_match);

#ifdef CONFIG_OF
static int max9296_parse_dt(struct max9296 *priv,
				struct i2c_client *client)
{
	struct device_node *node = client->dev.of_node;
	int err = 0;
	const char *str_value;
	int value;
	const struct of_device_id *match;

	if (!node)
		return -EINVAL;

	match = of_match_device(max9296_of_match, &client->dev);
	if (!match) {
		dev_err(&client->dev, "Failed to find matching dt id\n");
		return -EFAULT;
	}

	err = of_property_read_string(node, "csi-mode", &str_value);
	if (err < 0) {
		dev_err(&client->dev, "csi-mode property not found\n");
		return err;
	}

	if (!strcmp(str_value, "2x4")) {
		priv->csi_mode = MAX9296_CSI_MODE_2X4;
		priv->lane_mp1 = MAX9296_LANE_MAP1_2X4;
		priv->lane_mp2 = MAX9296_LANE_MAP2_2X4;
	} else if (!strcmp(str_value, "4x2")) {
		priv->csi_mode = MAX9296_CSI_MODE_4X2;
		priv->lane_mp1 = MAX9296_LANE_MAP1_4X2;
		priv->lane_mp2 = MAX9296_LANE_MAP2_4X2;
	} else {
		dev_err(&client->dev, "invalid csi mode\n");
		return -EINVAL;
	}

	err = of_property_read_u32(node, "max-src", &value);
	if (err < 0) {
		dev_err(&client->dev, "No max-src info\n");
		return err;
	}
	priv->max_src = value;

	priv->reset_gpio = of_get_named_gpio(node, "reset-gpios", 0);
	if (priv->reset_gpio < 0) {
		dev_err(&client->dev, "reset-gpios not found %d\n", err);
		return err;
	}

	/* digital 1.2v */
	if (of_get_property(node, "vdd_cam_1v2-supply", NULL)) {
		priv->vdd_cam_1v2 = regulator_get(&client->dev, "vdd_cam_1v2");
		if (IS_ERR(priv->vdd_cam_1v2)) {
			dev_err(&client->dev,
				"vdd_cam_1v2 regulator get failed\n");
			err = PTR_ERR(priv->vdd_cam_1v2);
			priv->vdd_cam_1v2 = NULL;
			return err;
		}
	} else {
		priv->vdd_cam_1v2 = NULL;
	}

	return 0;
}
#else
static int max9296_parse_pdata(struct max9296 *priv,
				struct i2c_client *client)
{
	struct max9296_pdata *pdata = client->dev.platform_data;
	if (pdata) {
		if (pdata->csi_mode == GMSL_CSI_2X4_MODE) {
			priv->csi_mode = MAX9296_CSI_MODE_2X4;
			priv->lane_mp1 = MAX9296_LANE_MAP1_2X4;
			priv->lane_mp2 = MAX9296_LANE_MAP2_2X4;
		} else if (pdata->csi_mode == GMSL_CSI_4X2_MODE) {
			priv->csi_mode = MAX9296_CSI_MODE_4X2;
			priv->lane_mp1 = MAX9296_LANE_MAP1_4X2;
			priv->lane_mp2 = MAX9296_LANE_MAP2_4X2;
		} else {
			dev_err(&client->dev, "invalid csi mode\n");
			return -EINVAL;
		}
		priv->max_src = pdata->max_src;
	} else {
		priv->csi_mode = MAX9296_CSI_MODE_2X4;
		priv->lane_mp1 = MAX9296_LANE_MAP1_2X4;
		priv->lane_mp2 = MAX9296_LANE_MAP2_2X4;
		priv->max_src = 1;
		priv->reset_gpio = 0;
		priv->vdd_cam_1v2 = NULL;
	}
	return 0;
}

#endif
static struct regmap_config max9296_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static int max9296_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct max9296 *priv;
	int err = 0;

	dev_info(&client->dev, "[MAX9296]: probing GMSL Deserializer\n");

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	priv->i2c_client = client;
	priv->regmap = devm_regmap_init_i2c(priv->i2c_client,
				&max9296_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(priv->regmap));
		return -ENODEV;
	}
#ifdef CONFIG_OF
	err = max9296_parse_dt(priv, client);
	if (err) {
		dev_err(&client->dev, "unable to parse dt\n");
		return -EFAULT;
	}
#else
	err = max9296_parse_pdata(priv, client);
	if (err) {
		dev_err(&client->dev, "unable to parse pdata\n");
		return -EFAULT;
	}
#endif
	max9296_pipes_reset(priv);

	if (priv->max_src > MAX9296_MAX_SOURCES) {
		dev_err(&client->dev,
			"max sources more than currently supported\n");
		return -EINVAL;
	}

	mutex_init(&priv->lock);

	dev_set_drvdata(&client->dev, priv);

	/* dev communication gets validated when GMSL link setup is done */
	dev_info(&client->dev, "%s:  success\n", __func__);

	return err;
}


static int max9296_remove(struct i2c_client *client)
{
	struct max9296 *priv;

	if (client != NULL) {
		priv = dev_get_drvdata(&client->dev);
		dev_info(&client->dev, "[MAX9296]: remove GMSL Deserializer\n");
		mutex_destroy(&priv->lock);
#ifdef CONFIG_OF
		i2c_unregister_device(client);
		client = NULL;
#endif
	}

	return 0;
}

static const struct i2c_device_id max9296_id[] = {
	{ "max9296", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, max9296_id);

static struct i2c_driver max9296_i2c_driver = {
	.driver = {
		.name = "max9296",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(max9296_of_match),
	},
	.probe = max9296_probe,
	.remove = max9296_remove,
	.id_table = max9296_id,
};

static int __init max9296_init(void)
{
	return i2c_add_driver(&max9296_i2c_driver);
}

static void __exit max9296_exit(void)
{
	i2c_del_driver(&max9296_i2c_driver);
}

module_init(max9296_init);
module_exit(max9296_exit);

MODULE_DESCRIPTION("Dual GMSL Deserializer driver max9296");
MODULE_AUTHOR("Sudhir Vyas <svyas@nvidia.com");
MODULE_LICENSE("GPL v2");
