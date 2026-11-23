/*
 * max96717.h - Maxim MAX96717 registers and constants.
 *
 * Copyright (c) 2022, D3 Engineering.  All rights reserved.
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

#ifndef _MAX96717_H_
#define _MAX96717_H_

#include <linux/bitops.h>
#include "serdes.h"

#define MAX96717_NAME "max96717"

enum max96717_gpio_out_type {
	MAX96717_GPIO_OUT_TYPE_OPEN_DRAIN = 0,
	MAX96717_GPIO_OUT_TYPE_PUSH_PULL = 1,
};

enum max96717_gpio_pull_updn_sel {
	MAX96717_GPIO_PULL_UPDN_SEL_NONE = 0,
	MAX96717_GPIO_PULL_UPDN_SEL_UP = 1,
	MAX96717_GPIO_PULL_UPDN_SEL_DOWN = 2,
};

#define MAX96717_NUM_SERIAL_LINKS 1
#define MAX96717_NUM_VIDEO_PIPES 1
#define MAX96717_NUM_MIPI_MAPS 1
#define MAX96717_NUM_CSI_LINKS 1
#define MAX96717_NUM_GPIO 11

#define MAX96717_GPIO(gpio) (0x2BE + ((gpio) * 3))
#define MAX96717_GPIO_A(gpio) (MAX96717_GPIO(gpio) + 0)
#define MAX96717_GPIO_A_OUT_DIS_FIELD BIT(0)
#define MAX96717_GPIO_A_TX_EN_FIELD BIT(1)
#define MAX96717_GPIO_A_RX_EN_FIELD BIT(2)
#define MAX96717_GPIO_A_IN_FIELD BIT(3)
#define MAX96717_GPIO_A_OUT_FIELD BIT(4)
#define MAX96717_GPIO_A_RES_CFG_FIELD BIT(7)
#define MAX96717_GPIO_B(gpio) (MAX96717_GPIO(gpio) + 1)
#define MAX96717_GPIO_B_TX_ID GENMASK(4, 0)
#define MAX96717_GPIO_B_OUT_TYPE_FIELD BIT(5)
#define MAX96717_GPIO_B_PULL_UPDN_SEL_FIELD GENMASK(7, 6)
#define MAX96717_GPIO_C(gpio) (MAX96717_GPIO(gpio) + 2)
#define MAX96717_GPIO_C_RX_ID GENMASK(4, 0)

/*
 * Some macros refer to a pipe despite not using it
 * this is to make code easier to port between drivers
 * in spite of the max96717 only having 1 video pipe
 */
#define MAX96717_FRONTTOP_0 (0x308)
#define MAX96717_FRONTTOP_0_SEL_CSI_FIELD(pipe_id) BIT(pipe_id + 2)
#define MAX96717_FRONTTOP_0_START_CSI_FIELD(csi_id) BIT((csi_id) + 5)
#define MAX96717_FRONTTOP_9 (0x311)
#define MAX96717_FRONTTOP_9_START_VIDEO_FIELD(pipe_id, csi_id) BIT((pipe_id + 2) + 4 * (csi_id + 1))
#define MAX96717_FRONTTOP_10 (0x312)
#define MAX96717_FRONTTOP_10_DBL8_FIELD(pipe_id) BIT(2)
#define MAX96717_FRONTTOP_11 (0x313)
#define MAX96717_FRONTTOP_11_DBL10_FIELD(pipe_id) BIT(2)
#define MAX96717_FRONTTOP_11_DBL12_FIELD(pipe_id) BIT(6)
#define MAX96717_FRONTTOP_16 (0x318)
#define MAX96717_FRONTTOP_16_FIELD GENMASK(5, 0)
#define MAX96717_FRONTTOP_16_ENABLE BIT(6)
#define MAX96717_FRONTTOP_2X(pipe_id) (0x31E)
#define MAX96717_FRONTTOP_2X_BPP_EN_FIELD BIT(5)
#define MAX96717_FRONTTOP_2X_BPP_FIELD GENMASK(4, 0)


#define MAX96717_MIPI_RX (0x330)
#define MAX96717_MIPI_RX_1 (MAX96717_MIPI_RX + 1)
#define MAX96717_MIPI_RX_1_SEL_CSI_LANES_FIELD(csi_id) (GENMASK(1, 0) << ((csi_id + 1) * 4))

#define MAX96717_EXT11 (0x383)
#define MAX96717_TUNNEL_MODE BIT(7)

#endif /* _MAX96717_H_ */
