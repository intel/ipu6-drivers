/*
 * max9295.h - Maxim MAX9295 registers and constants.
 *
 * Copyright (c) 2020, D3 Engineering. All rights reserved.
 * Copyright (c) 2023-2024, Define Design Deploy Corp. All rights reserved.
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
// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2025 Intel Corporation.

#ifndef _MAX9295_H_
#define _MAX9295_H_

#include <linux/bitops.h>
#include "serdes.h"

enum max9295_dev_id {
	MAX9295A = 0x91,
	MAX9295B = 0x93,
	MAX9295E = 0x9B
};

enum max9295_gpio_out_type {
	MAX9295_GPIO_OUT_TYPE_OPEN_DRAIN = 0,
	MAX9295_GPIO_OUT_TYPE_PUSH_PULL = 1,
};

enum max9295_gpio_pull_updn_sel {
	MAX9295_GPIO_PULL_UPDN_SEL_NONE = 0,
	MAX9295_GPIO_PULL_UPDN_SEL_UP = 1,
	MAX9295_GPIO_PULL_UPDN_SEL_DOWN = 2,
};

#define MAX9295_NUM_ALIASES 2 /* 2 per i2c bus */
#define MAX9295_NUM_SERIAL_LINKS 1
#define MAX9295_NUM_VIDEO_PIPES 4
#define MAX9295_NUM_MIPI_MAPS 16
#define MAX9295_NUM_CSI_LINKS 2 // Only 1 port, but it is technically Port B
#define MAX9295_NUM_GPIO 11
#define MAX9295_NUM_DATA_TYPES 4

#define MAX9295_REG0 (0x0)
#define MAX9295_REG0_DEV_ADDR_FIELD GENMASK(7, 1)

#define MAX9295_PHY_REM_CTRL (0x1)
#define MAX9295_PHY_REM_CTRL_DIS_FIELD BIT(4)
#define MAX9295_PHY_LOCAL_CTRL_DIS_FIELD BIT(5)

#define MAX9295_REG2 (0x2)
#define MAX9295_REG2_VID_TX_EN_FIELD(pipe_id) BIT((pipe_id) + 4)

#define MAX9295_DEV_ID (0xD)
#define MAX9295_DEV_REV (0xE)
#define MAX9295_DEV_REV_FIELD GENMASK(3, 0)
#define MAX9295_CTRL0 (0x10)
#define MAX9295_CTRL0_RST_ALL BIT(7)

#define MAX9295_CFGI_INFOFR_TR3 (0x7B)
#define MAX9295_CFGL_SPI_TR3 (0x83)
#define MAX9295_CFGC_CC_TR3 (0x8B)
#define MAX9295_CFGL_GPIO_TR3 (0x93)
#define MAX9295_CFGL_IIC_X (0xA3)
#define MAX9295_CFGL_IIC_Y (0xAB)
#define MAX9295_TR3_TX_SRC_ID GENMASK(2, 0)

#define MAX9295_VIDEO_TX0(pipe_id) (0x100 + (pipe_id) * 8)
#define MAX9295_VIDEO_TX0_AUTO_BPP_EN_FIELD BIT(3)
#define MAX9295_VIDEO_TX1(pipe_id) (0x101 + (pipe_id) * 8)
#define MAX9295_VIDEO_TX1_BPP_FIELD GENMASK(5, 0)

#define MAX9295_GPIO(gpio) (0x2BE + ((gpio) * 3))
#define MAX9295_GPIO_A(gpio) (MAX9295_GPIO(gpio) + 0)
#define MAX9295_GPIO_A_OUT_DIS_FIELD BIT(0)
#define MAX9295_GPIO_A_TX_EN_FIELD BIT(1)
#define MAX9295_GPIO_A_RX_EN_FIELD BIT(2)
#define MAX9295_GPIO_A_IN_FIELD BIT(3)
#define MAX9295_GPIO_A_OUT_FIELD BIT(4)
#define MAX9295_GPIO_A_RES_CFG_FIELD BIT(7)
#define MAX9295_GPIO_B(gpio) (MAX9295_GPIO(gpio) + 1)
#define MAX9295_GPIO_B_TX_ID GENMASK(4, 0)
#define MAX9295_GPIO_B_OUT_TYPE_FIELD BIT(5)
#define MAX9295_GPIO_B_PULL_UPDN_SEL_FIELD GENMASK(7, 6)
#define MAX9295_GPIO_C(gpio) (MAX9295_GPIO(gpio) + 2)
#define MAX9295_GPIO_C_RX_ID GENMASK(4, 0)

#define MAX9295_FRONTTOP_0 (0x308)
#define MAX9295_FRONTTOP_0_LINE_INFO BIT(6)
#define MAX9295_FRONTTOP_0_SEL_CSI_FIELD(pipe_id) BIT(pipe_id)
#define MAX9295_FRONTTOP_0_START_CSI_FIELD(csi_id) BIT((csi_id) + 4)
#define MAX9295_FRONTTOP_9 (0x311)
#define MAX9295_FRONTTOP_9_START_VIDEO_FIELD(pipe_id, csi_id) BIT((pipe_id) + 4 * (csi_id))

// Double loading mode
#define MAX9295_FRONTTOP_10 (0x312)
#define MAX9295_FRONTTOP_10_DBL8_FIELD(pipe_id) BIT(pipe_id)
#define MAX9295_FRONTTOP_11 (0x313)
#define MAX9295_FRONTTOP_11_DBL10_FIELD(pipe_id) BIT(pipe_id)
#define MAX9295_FRONTTOP_11_DBL12_FIELD(pipe_id) BIT((pipe_id) + 4)

#define MAX9295_MEM_DT_SEL(pipe_id, dt_slot) (0x314 + (dt_slot) / 2 * 0xC2 + 2 * (pipe_id) + (dt_slot))
#define MAX9295_MEM_DT_SEL_DT_FIELD GENMASK(5, 0)
#define MAX9295_MEM_DT_SEL_EN_FIELD BIT(6)

// Software BPP override (used for double loading mode and zero padding)
#define MAX9295_SOFT_BPP(pipe_id) (0x31C + (pipe_id))
#define MAX9295_SOFT_BPP_EN_FIELD BIT(5)
#define MAX9295_SOFT_BPP_FIELD GENMASK(4, 0)

#define MAX9295_MIPI_RX (0x330)
#define MAX9295_MIPI_RX_1 (MAX9295_MIPI_RX + 1)
#define MAX9295_MIPI_RX_1_SEL_CSI_LANES_FIELD(csi_id) (GENMASK(1, 0) << (csi_id * 4))

// I2C SRC/DST
#define MAX9295_I2C_SRC(i2c_id, n) ((i2c_id == 0 ? 0x42 : (0x550 + (4 * ((i2c_id) - 1)))) + (2 * (n)) + 0)
#define MAX9295_I2C_SRC_FIELD GENMASK(7, 1)
#define MAX9295_I2C_DST(i2c_id, n) ((i2c_id == 0 ? 0x42 : (0x550 + (4 * ((i2c_id) - 1)))) + (2 * (n)) + 1)
#define MAX9295_I2C_DST_FIELD GENMASK(7, 1)

// RCLK registers
#define MAX9295_REG6 (0x6)
#define MAX9295_RCLK_EN_FIELD BIT(5)
#define MAX9295_REG3 (0x3)
#define MAX9295_RCLK_SEL_FIELD GENMASK(1, 0)
#define MAX9295_REF_VTG0 (0x3F0)
#define MAX9295_REFGEN_PREDEF_FREQ_FIELD GENMASK(5, 4)
#define MAX9295_REFGEN_PREDEF_ALT_FIELD BIT(3)
#define MAX9295_REFGEN_EN_FIELD BIT(0)
#define MAX9295_REF_VTG1 (0x3F1)
#define MAX9295_PCLK_GPIO_FIELD GENMASK(5, 1)
#define MAX9295_PCLK_EN_FIELD BIT(0)

#endif /* _MAX9295_H_ */
