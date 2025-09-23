/*
 * max9296.h - Maxim 9296 registers and constants.
 *
 * Copyright (c) 2023-2024 Define Design Deploy Corp. All Rights reserved.
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

#ifndef _MAX9296_H_
#define _MAX9296_H_

#include <linux/bitops.h>
#include "serdes.h"

enum max9296_dev_id {
	MAX9296A = 0x94
};

enum max9296_phy_mode {
	MAX9296_MIPI_PHY_4X2 = BIT(0), // four 2-lane ports
	MAX9296_MIPI_PHY_1X4 = BIT(1), // one 4-lane (CSI2 is master (TODO: Which port???))
	MAX9296_MIPI_PHY_2X4 = BIT(2), // two 4-lane ports (CSI1 is master for port A, CSI2 for port B)
	MAX9296_MIPI_PHY_1X4A_22 = BIT(3), // one 4-lane (PHY0+PHY1, CSI1 for port A) port and two 2-lane ports
	MAX9296_MIPI_PHY_1X4B_22 = BIT(4), // one 4-lane (PHY2+PHY3, CSI2 for port B) port and two 2-lane ports
};

enum max9296_link_mode {
	MAX9296_LINK_AB,
	MAX9296_LINK_A,
	MAX9296_LINK_B,
	MAX9296_LINK_SPLIT
};

#define MAX9296_NUM_SERIAL_LINKS 2
#define MAX9296_NUM_VIDEO_PIPES 4
#define MAX9296_NUM_MIPI_MAPS 16
#define MAX9296_NUM_CSI_LINKS 4  /* Total Number of PHYs */
/* 2 CSI controllers, 2 PHYs per controller, and 2 lanes per PHY */

#define MAX9296_DEFAULT_SERIAL_LINK_TIMEOUT_MS 250

#define MAX9296_DPLL_FREQ_MHZ_MULTIPLE 100

#define MAX9296_FLD_OFS(n, bits_per_field, count) (((n) % (count)) * (bits_per_field))
#define MAX9296_OFFSET_GENMASK(offset, h, l) GENMASK(offset + h, offset + l)

#define MAX9296_CTRL0 0x10
#define MAX9296_CTRL0_AUTO_CFG_FIELD BIT(4)
#define MAX9296_CTRL0_LINK_CFG_FIELD GENMASK(1, 0)
#define MAX9296_CTRL0_RESET_LINK_FIELD BIT(6)	// One bit to reset whole link
#define MAX9296_CTRL0_RESET_ALL_FIELD BIT(7)
#define MAX9296_CTRL0_RESET_ONESHOT_FIELD BIT(5)

#define MAX9296_PHY_REM_CTRL (0x1)
#define MAX9296_PHY_REM_CTRL_TX_FIELD (GENMASK(1, 0) << 2)
#define MAX9296_PHY_REM_CTRL_RX_FIELD GENMASK(1, 0)
#define MAX9296_PHY_REM_CTRL_DIS_FIELD BIT(4)

/*
 *CTRL3(0x13) LINK_MODE is set to link A.
 */
#define MAX9296_PHY_LOCKED(link) (0x13) /* Based on link mode */
#define MAX9296_PHY_LOCKED_FIELD BIT(3)

#define MAX9296_VIDEO_PIPE_SEL(pipe) (0x50 + pipe)
#define MAX9296_VIDEO_PIPE_STR_SEL_FIELD GENMASK(1, 0)

#define MAX9296_VIDEO_PIPE_EN(pipe) (0x2)
#define MAX9296_VIDEO_PIPE_EN_FIELD(pipe) (BIT(pipe) << 4)

#define MAX9296_DPLL_FREQ(phy) (0x31D + ((phy) * 3))
#define MAX9296_DPLL_FREQ_FIELD GENMASK(4, 0)

#define MAX9296_MIPI_TX_EXT(pipe) (0x500 + ((pipe) * 0x10))

#define MAX9296_MIPI_PHY0 0x330
#define MAX9296_MIPI_PHY0_MODE_FIELD GENMASK(4, 0)
#define MAX9296_MIPI_PHY_ENABLE 0x332
#define MAX9296_MIPI_PHY_ENABLE_FIELD(csi) BIT((csi) + 4)
#define MAX9296_MIPI_PHY_LANE_MAP(csi) (0x333 + (csi) / 2)
#define MAX9296_MIPI_PHY_LANE_MAP_FIELD(csi, lane)	\
	(GENMASK(1, 0) << (MAX9296_FLD_OFS(csi, 4, 2) + MAX9296_FLD_OFS(lane, 2, 2)))

// Note that CSIs and pipes overlap:
#define MAX9296_MIPI_TX(pipe) (0x400 + ((pipe) * 0x40))

/* Offsets for MAX9296_MIPI_TX under 0x0B are only for lanes 1 and 3 */
#define MAX9296_MIPI_TX_LANE_CNT(csi) (MAX9296_MIPI_TX(csi) + 0x0A)
#define MAX9296_MIPI_TX_LANE_CNT_FIELD GENMASK(7, 6)
#define MAX9296_MIPI_TX_DESKEW_INIT(csi) (MAX9296_MIPI_TX(csi) + 0x03)
#define MAX9296_MIPI_TX_DESKEW_INIT_AUTO_EN BIT(7)
#define MAX9296_MIPI_TX_DESKEW_INIT_WIDTH GENMASK(2, 0)
#define MAX9296_MAP_EN_L(pipe) (MAX9296_MIPI_TX(pipe) + 0x0B)
#define MAX9296_MAP_EN_H(pipe) (MAX9296_MIPI_TX(pipe) + 0x0C)
#define MAX9296_MAP_EN_FIELD GENMASK(7, 0)
#define MAX9296_MAP_SRC_L(pipe, map) (MAX9296_MIPI_TX(pipe) + 0x0D + ((map) * 2))
#define MAX9296_MAP_SRC_L_VC_FIELD GENMASK(7, 6)
#define MAX9296_MAP_SRC_L_DT_FIELD GENMASK(5, 0)
#define MAX9296_MAP_DST_L(pipe, map) (MAX9296_MIPI_TX(pipe) + 0x0D + ((map) * 2) + 1)
#define MAX9296_MAP_DST_L_VC_FIELD GENMASK(7, 6)
#define MAX9296_MAP_DST_L_DT_FIELD GENMASK(5, 0)
#define MAX9296_MAP_SRCDST_H(pipe, map) (MAX9296_MIPI_TX_EXT(pipe) + (map))
#define MAX9296_MAP_SRCDST_H_SRC_VC_FIELD GENMASK(7, 5)
#define MAX9296_MAP_SRCDST_H_DST_VC_FIELD GENMASK(4, 2)
#define MAX9296_MAP_DPHY_DEST(pipe, map) (MAX9296_MIPI_TX(pipe) + 0x2D + ((map) / 4))
#define MAX9296_MAP_DPHY_DEST_FIELD(map) (GENMASK(1, 0) << MAX9296_FLD_OFS(map, 2, 4))

#define MAX9296_MIPI_TX_ALT_MEM(csi) (MAX9296_MIPI_TX(csi) + 0x33)
#define MAX9296_MIPI_TX_ALT_MEM_FIELD GENMASK(2, 0)
#define MAX9296_MIPI_TX_ALT_MEM_8BPP BIT(1)
#define MAX9296_MIPI_TX_ALT_MEM_10BPP BIT(2)
#define MAX9296_MIPI_TX_ALT_MEM_12BPP BIT(0)

#define MAX9296_GPIO_A(gpio_num) (0x2B0 + 3 * gpio_num)
#define MAX9296_GPIO_B(gpio_num) (MAX9296_GPIO_A(gpio_num) + 1)
#define MAX9296_GPIO_B_TX_ID GENMASK(4, 0)
#define MAX9296_GPIO_C(gpio_num) (MAX9296_GPIO_A(gpio_num) + 2)
#define MAX9296_GPIO_C_RX_ID GENMASK(4, 0)

#define MAX9296_DPLL_RESET(phy) (0x1C00 + ((phy) * 0x100))
#define MAX9296_DPLL_RESET_SOFT_RST_FIELD BIT(0)

static struct max9x_serdes_rate_table max9296_rx_rates[] = {
	{ .val = 1, .freq_mhz = 3000}, // 3 GHz
	{ .val = 2, .freq_mhz = 6000}, // 6 GHz
};

static struct max9x_serdes_rate_table max9296_tx_rates[] = {
	{ .val = 0, .freq_mhz = 187}, // 187.5 MHz
};

#endif /* _MAX9296_H_ */
