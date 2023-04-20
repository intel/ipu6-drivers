/*
 * max9295.c - max9295 GMSL Serializer driver
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

// #include <media/camera_common.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <media/max9295.h>

/* register specifics */
#define MAX9295_MIPI_RX0_ADDR 0x330
#define MAX9295_MIPI_RX1_ADDR 0x331
#define MAX9295_MIPI_RX2_ADDR 0x332
#define MAX9295_MIPI_RX3_ADDR 0x333

#define MAX9295_PIPE_X_DT_ADDR 0x314
#define MAX9295_PIPE_Y_DT_ADDR 0x316
#define MAX9295_PIPE_Z_DT_ADDR 0x318
#define MAX9295_PIPE_U_DT_ADDR 0x31A

#define MAX9295_CTRL0_ADDR 0x10
#define MAX9295_SRC_CTRL_ADDR 0x2BF
#define MAX9295_SRC_PWDN_ADDR 0x02BE
#define MAX9295_SRC_OUT_RCLK_ADDR 0x3F1
#define MAX9295_START_PIPE_ADDR 0x311
#define MAX9295_PIPE_EN_ADDR 0x2
#define MAX9295_CSI_PORT_SEL_ADDR 0x308

#define MAX9295_I2C4_ADDR 0x44
#define MAX9295_I2C5_ADDR 0x45

#define MAX9295_DEV_ADDR 0x00

#define MAX9295_STREAM_PIPE_UNUSED 0x22
#define MAX9295_CSI_MODE_1X4 0x00
#define MAX9295_CSI_MODE_2X2 0x03
#define MAX9295_CSI_MODE_2X4 0x06

#define MAX9295_CSI_PORT_B(num_lanes) (((num_lanes) << 4) & 0xF0)
#define MAX9295_CSI_PORT_A(num_lanes) ((num_lanes) & 0x0F)

#define MAX9295_CSI_1X4_MODE_LANE_MAP1 0xE0
#define MAX9295_CSI_1X4_MODE_LANE_MAP2 0x04

#define MAX9295_CSI_2X4_MODE_LANE_MAP1 0xEE
#define MAX9295_CSI_2X4_MODE_LANE_MAP2 0xE4

#define MAX9295_CSI_2X2_MODE_LANE_MAP1 MAX9295_CSI_2X4_MODE_LANE_MAP1
#define MAX9295_CSI_2X2_MODE_LANE_MAP2 MAX9295_CSI_2X4_MODE_LANE_MAP2

#define MAX9295_ST_ID_0 0x0
#define MAX9295_ST_ID_1 0x1
#define MAX9295_ST_ID_2 0x2
#define MAX9295_ST_ID_3 0x3

#define MAX9295_PIPE_X_START_B 0x80
#define MAX9295_PIPE_Y_START_B 0x40
#define MAX9295_PIPE_Z_START_B 0x20
#define MAX9295_PIPE_U_START_B 0x10

#define MAX9295_PIPE_X_START_A 0x1
#define MAX9295_PIPE_Y_START_A 0x2
#define MAX9295_PIPE_Z_START_A 0x4
#define MAX9295_PIPE_U_START_A 0x8

#define MAX9295_START_PORT_A 0x10
#define MAX9295_START_PORT_B 0x20

#define MAX9295_CSI_LN2 0x1
#define MAX9295_CSI_LN4 0x3

#define MAX9295_EN_LINE_INFO 0x40

#define MAX9295_VID_TX_EN_X 0x10
#define MAX9295_VID_TX_EN_Y 0x20
#define MAX9295_VID_TX_EN_Z 0x40
#define MAX9295_VID_TX_EN_U 0x80

#define MAX9295_VID_INIT 0x3
#define MAX9295_SRC_RCLK 0x89

#define MAX9295_RESET_ALL 0x80
#define MAX9295_RESET_SRC 0x60
#define MAX9295_PWDN_GPIO 0x90

#define MAX9295_MAX_PIPES 0x4

struct max9295_client_ctx {
	struct gmsl_link_ctx *g_ctx;
	bool st_done;
};

struct max9295 {
	struct i2c_client *i2c_client;
	struct regmap *regmap;
	struct max9295_client_ctx g_client;
	struct mutex lock;
	/* primary serializer properties */
	__u32 def_addr;
	__u32 pst2_ref;
};

static struct max9295 *prim_priv__;

struct map_ctx {
	u8 dt;
	u16 addr;
	u8 val;
	u8 st_id;
};

static int max9295_write_reg(struct device *dev, u16 addr, u8 val)
{
	struct max9295 *priv = dev_get_drvdata(dev);
	int err;

	err = regmap_write(priv->regmap, addr, val);
	if (err)
		dev_err(dev, "%s:i2c write failed, 0x%x = %x\n",
			__func__, addr, val);

	/* delay before next i2c command as required for SERDES link */
	usleep_range(100, 110);

	return err;
}

int max9295_setup_streaming(struct device *dev)
{
	struct max9295 *priv = dev_get_drvdata(dev);
	int err = 0;
	u32 csi_mode;
	u32 lane_map1;
	u32 lane_map2;
	u32 port;
	u32 rx1_lanes;
	u32 st_pipe;
	u32 pipe_en;
	u32 port_sel = 0;
	struct gmsl_link_ctx *g_ctx;
	u32 i;
	u32 j;
	u32 st_en;

	struct map_ctx map_pipe_dtype[] = {
		{GMSL_CSI_DT_RAW_12, MAX9295_PIPE_Z_DT_ADDR, 0x2C,
			MAX9295_ST_ID_2},
		{GMSL_CSI_DT_UED_U1, MAX9295_PIPE_X_DT_ADDR, 0x30,
			MAX9295_ST_ID_0},
		{GMSL_CSI_DT_EMBED, MAX9295_PIPE_Y_DT_ADDR, 0x12,
			MAX9295_ST_ID_1},
	};

	mutex_lock(&priv->lock);

	if (!priv->g_client.g_ctx) {
		dev_err(dev, "%s: no sdev client found\n", __func__);
		err = -EINVAL;
		goto error;
	}

	if (priv->g_client.st_done) {
		dev_dbg(dev, "%s: stream setup is already done\n", __func__);
		goto error;
	}

	g_ctx = priv->g_client.g_ctx;

	switch (g_ctx->csi_mode) {
	case GMSL_CSI_1X4_MODE:
		csi_mode = MAX9295_CSI_MODE_1X4;
		lane_map1 = MAX9295_CSI_1X4_MODE_LANE_MAP1;
		lane_map2 = MAX9295_CSI_1X4_MODE_LANE_MAP2;
		rx1_lanes = MAX9295_CSI_LN4;
		break;
	case GMSL_CSI_2X2_MODE:
		csi_mode = MAX9295_CSI_MODE_2X2;
		lane_map1 = MAX9295_CSI_2X2_MODE_LANE_MAP1;
		lane_map2 = MAX9295_CSI_2X2_MODE_LANE_MAP2;
		rx1_lanes = MAX9295_CSI_LN2;
		break;
	case GMSL_CSI_2X4_MODE:
		csi_mode = MAX9295_CSI_MODE_2X4;
		lane_map1 = MAX9295_CSI_2X4_MODE_LANE_MAP1;
		lane_map2 = MAX9295_CSI_2X4_MODE_LANE_MAP2;
		rx1_lanes = MAX9295_CSI_LN4;
		break;
	default:
		dev_err(dev, "%s: invalid csi mode\n", __func__);
		err = -EINVAL;
		goto error;
	}

	port = (g_ctx->src_csi_port == GMSL_CSI_PORT_B) ?
			MAX9295_CSI_PORT_B(rx1_lanes) :
			MAX9295_CSI_PORT_A(rx1_lanes);

	max9295_write_reg(dev, MAX9295_MIPI_RX0_ADDR, csi_mode);
	max9295_write_reg(dev, MAX9295_MIPI_RX1_ADDR, port);
	max9295_write_reg(dev, MAX9295_MIPI_RX2_ADDR, lane_map1);
	max9295_write_reg(dev, MAX9295_MIPI_RX3_ADDR, lane_map2);

	for (i = 0; i < g_ctx->num_streams; i++) {
		struct gmsl_stream *g_stream = &g_ctx->streams[i];

		g_stream->st_id_sel = GMSL_ST_ID_UNUSED;
		for (j = 0; j < ARRAY_SIZE(map_pipe_dtype); j++) {
			if (map_pipe_dtype[j].dt == g_stream->st_data_type) {
				/*
				 * TODO:
				 * 1) Remove link specific overrides, depends
				 * on #2.
				 * 2) Add support for vc id based stream sel
				 * overrides TX_SRC_SEL. would be useful in
				 * using same mappings in all ser devs.
				 */
				if (g_ctx->serdes_csi_link ==
					GMSL_SERDES_CSI_LINK_B) {
					map_pipe_dtype[j].addr += 2;
					map_pipe_dtype[j].st_id += 1;
				}

				g_stream->st_id_sel = map_pipe_dtype[j].st_id;
				st_en = (map_pipe_dtype[j].addr ==
						MAX9295_PIPE_X_DT_ADDR) ?
							0xC0 : 0x40;

				max9295_write_reg(dev, map_pipe_dtype[j].addr,
					(st_en | map_pipe_dtype[j].val));
			}
		}
	}

	for (i = 0; i < g_ctx->num_streams; i++)
		if (g_ctx->streams[i].st_id_sel != GMSL_ST_ID_UNUSED)
			port_sel |= (1 << g_ctx->streams[i].st_id_sel);

	if (g_ctx->src_csi_port == GMSL_CSI_PORT_B) {
		st_pipe = (MAX9295_PIPE_X_START_B | MAX9295_PIPE_Y_START_B |
			MAX9295_PIPE_Z_START_B | MAX9295_PIPE_U_START_B);
		port_sel |= (MAX9295_EN_LINE_INFO | MAX9295_START_PORT_B);
	} else {
		st_pipe = MAX9295_PIPE_X_START_A | MAX9295_PIPE_Y_START_A |
			MAX9295_PIPE_Z_START_A | MAX9295_PIPE_U_START_A;
		port_sel |= (MAX9295_EN_LINE_INFO | MAX9295_START_PORT_A);
	}

	pipe_en = (MAX9295_VID_TX_EN_X | MAX9295_VID_TX_EN_Y |
		MAX9295_VID_TX_EN_Z | MAX9295_VID_TX_EN_U | MAX9295_VID_INIT);

	max9295_write_reg(dev, MAX9295_START_PIPE_ADDR, st_pipe);
	max9295_write_reg(dev, MAX9295_CSI_PORT_SEL_ADDR, port_sel);
	max9295_write_reg(dev, MAX9295_PIPE_EN_ADDR, pipe_en);

	priv->g_client.st_done = true;

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max9295_setup_streaming);

int max9295_setup_control(struct device *dev)
{
	struct max9295 *priv = dev_get_drvdata(dev);
	int err = 0;
	struct gmsl_link_ctx *g_ctx;
	u32 offset1 = 0;
	u32 offset2 = 0;
	u32 i;

	u8 i2c_ovrd[] = {
		0x6B, 0x10,
		0x73, 0x11,
		0x7B, 0x30,
		0x83, 0x30,
		0x93, 0x30,
		0x9B, 0x30,
		0xA3, 0x30,
		0xAB, 0x30,
		0x8B, 0x30,
	};

	u8 addr_offset[] = {
		0x80, 0x00, 0x00,
		0x84, 0x00, 0x01,
		0x86, 0x00, 0x01,
		0x88, 0x00, 0x01,
		0x8A, 0x00, 0x01,
		0xC0, 0x02, 0x02,
		0xC4, 0x02, 0x03,
		0xC6, 0x02, 0x03,
		0xC8, 0x02, 0x03,
		0xCA, 0x02, 0x03,
	};

	mutex_lock(&priv->lock);

	if (!priv->g_client.g_ctx) {
		dev_err(dev, "%s: no sensor dev client found\n", __func__);
		err = -EINVAL;
		goto error;
	}

	g_ctx = priv->g_client.g_ctx;

#ifdef CONFIG_OF
	if (prim_priv__) {
		/* update address reassignment */
		max9295_write_reg(&prim_priv__->i2c_client->dev,
				MAX9295_DEV_ADDR, (g_ctx->ser_reg << 1));
	}
#else
	if (priv->def_addr) {
		/* update address reassignment */
		struct i2c_client *c = to_i2c_client(dev);
		int addr = c->addr;
		dev_info(dev, "%s: update address reassignment 0x%x->0x%x\n", __func__,priv->def_addr, g_ctx->ser_reg);
		c->addr = priv->def_addr;
		max9295_write_reg(dev,
				MAX9295_DEV_ADDR, (g_ctx->ser_reg << 1));
		c->addr = addr;
		prim_priv__ = priv;
	} else {
		prim_priv__ = NULL;
	}
#endif
	if (g_ctx->serdes_csi_link == GMSL_SERDES_CSI_LINK_A)
		err = max9295_write_reg(dev, MAX9295_CTRL0_ADDR, 0x21);
	else
		err = max9295_write_reg(dev, MAX9295_CTRL0_ADDR, 0x22);

	/* check if serializer device exists */
	if (err) {
		dev_err(dev, "%s: ERROR: ser device not found\n", __func__);
		goto error;
	}

	/* delay to settle link */
	msleep(100);

	for (i = 0; i < ARRAY_SIZE(addr_offset); i += 3) {
		if ((g_ctx->ser_reg << 1) == addr_offset[i]) {
			offset1 = addr_offset[i+1];
			offset2 = addr_offset[i+2];
			break;
		}
	}

	if (i == ARRAY_SIZE(addr_offset)) {
		dev_err(dev, "%s: invalid ser slave address\n", __func__);
		err = -EINVAL;
		goto error;
	}

	for (i = 0; i < ARRAY_SIZE(i2c_ovrd); i += 2) {
		/* update address overrides */
		i2c_ovrd[i+1] += (i < 4) ? offset1 : offset2;

		/* i2c passthrough2 must be configured once for all devices */
		if ((i2c_ovrd[i] == 0x8B) && prim_priv__ &&
				prim_priv__->pst2_ref)
			continue;

		max9295_write_reg(dev, i2c_ovrd[i], i2c_ovrd[i+1]);
	}

	/* dev addr pass-through2 ref */
	if (prim_priv__)
		prim_priv__->pst2_ref++;

	max9295_write_reg(dev, MAX9295_I2C4_ADDR, (g_ctx->sdev_reg << 1));
	max9295_write_reg(dev, MAX9295_I2C5_ADDR, (g_ctx->sdev_def << 1));
	msleep(100);
	max9295_write_reg(dev, MAX9295_SRC_PWDN_ADDR, MAX9295_PWDN_GPIO);
	max9295_write_reg(dev, MAX9295_SRC_CTRL_ADDR, MAX9295_RESET_SRC);
	max9295_write_reg(dev, MAX9295_SRC_OUT_RCLK_ADDR, MAX9295_SRC_RCLK);

	g_ctx->serdev_found = true;

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max9295_setup_control);

int max9295_reset_control(struct device *dev)
{
	struct max9295 *priv = dev_get_drvdata(dev);
	int err = 0;

	mutex_lock(&priv->lock);
	if (!priv->g_client.g_ctx) {
		dev_err(dev, "%s: no sdev client found\n", __func__);
		err = -EINVAL;
		goto error;
	}

	priv->g_client.st_done = false;
#ifndef CONFIG_OF
	if (priv->def_addr) {
		prim_priv__ = priv;
	} else {
		prim_priv__ = NULL;
	}
#endif
	if (prim_priv__) {
		prim_priv__->pst2_ref--;

		max9295_write_reg(dev, MAX9295_DEV_ADDR,
					(prim_priv__->def_addr << 1));
		if (prim_priv__->pst2_ref == 0)
#ifdef CONFIG_OF
			max9295_write_reg(&prim_priv__->i2c_client->dev,
						MAX9295_CTRL0_ADDR, MAX9295_RESET_ALL);
#else
			if (priv->def_addr) {
				/* update address reassingment */
				struct i2c_client *c = to_i2c_client(dev);
				int addr = c->addr;
				c->addr = priv->def_addr;
				max9295_write_reg(dev,
						MAX9295_CTRL0_ADDR, MAX9295_RESET_ALL);
				c->addr = addr;
			}
#endif
	}

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max9295_reset_control);

int max9295_sdev_pair(struct device *dev, struct gmsl_link_ctx *g_ctx)
{
	struct max9295 *priv;
	int err = 0;

	if (!dev || !g_ctx || !g_ctx->s_dev) {
		dev_err(dev, "%s: invalid input params\n", __func__);
		return -EINVAL;
	}

	priv = dev_get_drvdata(dev);
	mutex_lock(&priv->lock);
	if (priv->g_client.g_ctx) {
		dev_err(dev, "%s: device already paired\n", __func__);
		err = -EINVAL;
		goto error;
	}

	priv->g_client.st_done = false;

	priv->g_client.g_ctx = g_ctx;

error:
	mutex_unlock(&priv->lock);
	return 0;
}
EXPORT_SYMBOL(max9295_sdev_pair);

int max9295_sdev_unpair(struct device *dev, struct device *s_dev)
{
	struct max9295 *priv = NULL;
	int err = 0;

	if (!dev || !s_dev) {
		dev_err(dev, "%s: invalid input params\n", __func__);
		return -EINVAL;
	}

	priv = dev_get_drvdata(dev);

	mutex_lock(&priv->lock);

	if (!priv->g_client.g_ctx) {
		dev_err(dev, "%s: device is not paired\n", __func__);
		err = -ENOMEM;
		goto error;
	}

	if (priv->g_client.g_ctx->s_dev != s_dev) {
		dev_err(dev, "%s: invalid device\n", __func__);
		err = -EINVAL;
		goto error;
	}

	priv->g_client.g_ctx = NULL;
	priv->g_client.st_done = false;

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max9295_sdev_unpair);

static  struct regmap_config max9295_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

struct reg_pair {
	u16 addr;
	u8 val;
};

static int max9295_set_registers(struct device *dev, struct reg_pair *map,
				 u32 count)
{
	int err = 0;
	u32 j = 0;

	for (j = 0; j < count; j++) {
		err = max9295_write_reg(dev,
			map[j].addr, map[j].val);
		if (err != 0)
			break;
	}

	return err;
}

static int __max9295_set_pipe(struct device *dev, int pipe_id, u8 data_type1,
			      u8 data_type2, u32 vc_id)
{
	int err = 0;
	u8 bpp = 0x30;
	static u8 pipe_x_val = 0x0;
	struct reg_pair map_multi_pipe_en[] = {
		{0x0315, 0x80},
	};
	struct reg_pair map_bpp8dbl[] = {
		{0x0312, 0x0F},
	};
	struct reg_pair map_pipe_control[] = {
		/* addr, val */
		{MAX9295_PIPE_X_DT_ADDR, 0x5E}, // Pipe X pulls data_type1
		{0x0315, 0x52}, // Pipe X pulls data_type2
		{0x0309, 0x01}, // # Pipe X pulls vc_id
		{0x030A, 0x00},
		{0x031C, 0x30}, // BPP in pipe X
		{0x0102, 0x0E}, // LIM_HEART Pipe X: Disabled
	};

	if (data_type1 == GMSL_CSI_DT_RAW_8 || data_type1 == GMSL_CSI_DT_EMBED
	    || data_type2 == GMSL_CSI_DT_RAW_8 || data_type2 == GMSL_CSI_DT_EMBED) {
		map_bpp8dbl[0].val |= (1 << pipe_id);
	} else {
		map_bpp8dbl[0].val &= ~(1 << pipe_id);
	}
	err |= max9295_set_registers(dev, map_bpp8dbl, ARRAY_SIZE(map_bpp8dbl));

	if (data_type1 == GMSL_CSI_DT_RGB_888)
		bpp = 0x18;

	map_pipe_control[0].addr += 0x2 * pipe_id;
	map_pipe_control[1].addr += 0x2 * pipe_id;
	map_pipe_control[2].addr += 0x2 * pipe_id;
	map_pipe_control[3].addr += 0x2 * pipe_id;
	map_pipe_control[4].addr += 0x1 * pipe_id;
	map_pipe_control[5].addr += 0x8 * pipe_id;

	map_pipe_control[0].val = 0x40 | data_type1;
	map_pipe_control[1].val = 0x40 | data_type2;
	map_pipe_control[2].val = 1 << vc_id;
	map_pipe_control[3].val = 0x00;
	map_pipe_control[4].val = bpp;
	map_pipe_control[5].val = 0x0E;

	if (pipe_id == 0)
		pipe_x_val = map_pipe_control[1].val;

	err |= max9295_set_registers(dev, map_pipe_control,
				     ARRAY_SIZE(map_pipe_control));

	map_multi_pipe_en[0].val = 0x80 | pipe_x_val;
	err |= max9295_set_registers(dev, map_multi_pipe_en,
				     ARRAY_SIZE(map_multi_pipe_en));

	return err;
}

int max9295_init_settings(struct device *dev)
{
	int err = 0;
	int i;
	struct max9295 *priv = dev_get_drvdata(dev);

	struct reg_pair map_pipe_opt[] = {
		// Enable all pipes
		{MAX9295_PIPE_EN_ADDR, 0xF3},
		// Write 0x33 for 4 lanes
		{MAX9295_MIPI_RX1_ADDR, 0x11},
		// All pipes pull clock from port B
		{MAX9295_CSI_PORT_SEL_ADDR, 0x6F},
		// All pipes pull data from port B
		{MAX9295_START_PIPE_ADDR, 0xF0},
	};

	mutex_lock(&priv->lock);

	// Init control
	err |= max9295_set_registers(dev, map_pipe_opt,
				     ARRAY_SIZE(map_pipe_opt));

	for (i = 0; i < MAX9295_MAX_PIPES; i++)
		err |= __max9295_set_pipe(dev, i, GMSL_CSI_DT_YUV422_8,
					  GMSL_CSI_DT_EMBED, i);

	mutex_unlock(&priv->lock);

	return err;
}
EXPORT_SYMBOL(max9295_init_settings);

int max9295_set_pipe(struct device *dev, int pipe_id,
		     u8 data_type1, u8 data_type2, u32 vc_id)
{
	struct max9295 *priv = dev_get_drvdata(dev);
	int err = 0;

	if (pipe_id > (MAX9295_MAX_PIPES - 1)) {
		dev_info(dev, "%s, input pipe_id: %d exceed max9295 max pipes\n",
			 __func__, pipe_id);
		return -EINVAL;
	}

	dev_dbg(dev, "%s pipe_id %d, data_type1 %u, data_type2 %u, vc_id %u\n",
		__func__, pipe_id, data_type1, data_type2, vc_id);

	mutex_lock(&priv->lock);

	err = __max9295_set_pipe(dev, pipe_id, data_type1, data_type2, vc_id);

	mutex_unlock(&priv->lock);

	return err;
}
EXPORT_SYMBOL(max9295_set_pipe);

static int max9295_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct max9295 *priv;
	struct max9295_pdata *pdata = client->dev.platform_data;
	int err = 0;
#ifdef CONFIG_OF
	struct device_node *node = client->dev.of_node;
#endif

	dev_info(&client->dev, "[MAX9295]: probing GMSL Serializer\n");

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	priv->i2c_client = client;
	priv->regmap = devm_regmap_init_i2c(priv->i2c_client,
				&max9295_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(priv->regmap));
		return -ENODEV;
	}

	mutex_init(&priv->lock);
#ifdef CONFIG_OF
	if (of_get_property(node, "is-prim-ser", NULL)) {
		if (prim_priv__) {
			dev_err(&client->dev,
				"prim-ser already exists\n");
				return -EEXIST;
		}

		err = of_property_read_u32(node, "reg", &priv->def_addr);
		if (err < 0) {
			dev_err(&client->dev, "reg not found\n");
			return -EINVAL;
		}

		prim_priv__ = priv;
	}
#else
	if (pdata->is_prim_ser) {
		prim_priv__ = priv;
		priv->def_addr = pdata->def_addr;
	} else {
		priv->def_addr = 0;
	}
#endif
	dev_set_drvdata(&client->dev, priv);

	/* dev communication gets validated when GMSL link setup is done */
	dev_info(&client->dev, "%s:  success\n", __func__);

	return err;
}

static int max9295_remove(struct i2c_client *client)
{
	struct max9295 *priv;

	if (client != NULL) {
		priv = dev_get_drvdata(&client->dev);
		dev_info(&client->dev, "[MAX9295]: remove GMSL Serializer\n");
		mutex_destroy(&priv->lock);
#ifdef CONFIG_OF
		i2c_unregister_device(client);
		client = NULL;
#endif
	}

	return 0;
}

static const struct i2c_device_id max9295_id[] = {
	{ "max9295", 0 },
	{ },
};

static const struct of_device_id max9295_of_match[] = {
	{ .compatible = "maxim,max9295", },
	{ },
};
MODULE_DEVICE_TABLE(of, max9295_of_match);
MODULE_DEVICE_TABLE(i2c, max9295_id);

static struct i2c_driver max9295_i2c_driver = {
	.driver = {
		.name = "max9295",
		.owner = THIS_MODULE,
	},
	.probe = max9295_probe,
	.remove = max9295_remove,
	.id_table = max9295_id,
};

static int __init max9295_init(void)
{
	return i2c_add_driver(&max9295_i2c_driver);
}

static void __exit max9295_exit(void)
{
	i2c_del_driver(&max9295_i2c_driver);
}

module_init(max9295_init);
module_exit(max9295_exit);

MODULE_DESCRIPTION("GMSL Serializer driver max9295");
MODULE_AUTHOR("Sudhir Vyas <svyas@nvidia.com>");
MODULE_AUTHOR("Dmitry Perchanov <dmitry.perchanov@intel.com>");
MODULE_LICENSE("GPL v2");
