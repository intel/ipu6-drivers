// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2022-2024 Intel Corporation
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/pci.h>

#include <media/ipu-isys.h>
#if IS_ENABLED(CONFIG_VIDEO_TI960)
#include <media/ti960.h>
#endif
#if IS_ENABLED(CONFIG_VIDEO_AR0234)
#include <media/ar0234.h>
#endif
#if IS_ENABLED(CONFIG_VIDEO_IMX390)
#include <media/imx390.h>
#endif
#if IS_ENABLED(CONFIG_VIDEO_ISX031)
#include <media/isx031.h>
#endif
#if IS_ENABLED(CONFIG_VIDEO_LT6911UXC)
#include <media/lt6911uxc.h>
#endif
#if IS_ENABLED(CONFIG_VIDEO_LT6911UXE)
#include <media/lt6911uxe.h>
#endif
#if IS_ENABLED(CONFIG_VIDEO_D4XX)
#include <media/d4xx_pdata.h>
#endif

#include "ipu.h"

#if IS_ENABLED(CONFIG_VIDEO_AR0234)
#define AR0234_LANES       2
#define AR0234_I2C_ADDRESS 0x10

#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA) \
	&& IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING)
static void ar0234_fixup_spdata(const void *spdata_rep, void *spdata)
{
	const struct ipu_spdata_rep *rep = spdata_rep;
	struct ar0234_platform_data *platform = spdata;

	if (spdata_rep && spdata) {
		platform->port = rep->port_n;
		platform->lanes = rep->lanes;
		platform->i2c_slave_address = rep->slave_addr_n;
		platform->gpios[0] = rep->gpios[0];
		platform->irq_pin = rep->irq_pin;
		platform->irq_pin_flags = rep->irq_pin_flags;
		strcpy(platform->irq_pin_name, rep->irq_pin_name);
		platform->suffix = rep->suffix;
	}
}
#endif

static struct ipu_isys_csi2_config ar0234_csi2_cfg_1 = {
	.nlanes = AR0234_LANES,
	.port = 1,
};

static struct ar0234_platform_data ar0234_pdata_1 = {
	.port = 1,
	.lanes = 2,
	.i2c_slave_address = AR0234_I2C_ADDRESS,
	.irq_pin = -1,
	.irq_pin_name = "",
	.irq_pin_flags = IRQF_TRIGGER_RISING
		| IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	.suffix = 'a',
	.gpios = {-1, 0, 0, 0},
};

static struct ipu_isys_subdev_info ar0234_sd_1 = {
	.csi2 = &ar0234_csi2_cfg_1,
	.i2c = {
	.board_info = {
		I2C_BOARD_INFO("ar0234", AR0234_I2C_ADDRESS),
		.platform_data = &ar0234_pdata_1,
	},
	.i2c_adapter_bdf = "0000:00:15.1",
	},
#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA) \
	&& IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING)
	.fixup_spdata = ar0234_fixup_spdata,
#endif
};

static struct ipu_isys_csi2_config ar0234_csi2_cfg_2 = {
	.nlanes = AR0234_LANES,
	.port = 2,
};

static struct ar0234_platform_data ar0234_pdata_2 = {
	.port = 2,
	.lanes = 2,
	.i2c_slave_address = AR0234_I2C_ADDRESS,
	.irq_pin = -1,
	.irq_pin_name = "",
	.irq_pin_flags = IRQF_TRIGGER_RISING
		| IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	.suffix = 'b',
	.gpios = {-1, 0, 0, 0},
};

static struct ipu_isys_subdev_info ar0234_sd_2 = {
	.csi2 = &ar0234_csi2_cfg_2,
	.i2c = {
	.board_info = {
		I2C_BOARD_INFO("ar0234", AR0234_I2C_ADDRESS),
		.platform_data = &ar0234_pdata_2,
	},
	.i2c_adapter_bdf = "0000:00:19.1",
	},
#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA) \
	&& IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING)
	.fixup_spdata = ar0234_fixup_spdata,
#endif
};
#endif

#if IS_ENABLED(CONFIG_VIDEO_IMX390)
#define IMX390_LANES       4
#define IMX390_D3RCM_I2C_ADDRESS 0x1a
#define IMX390_I2C_ADDRESS_3 0x1e
#define IMX390_I2C_ADDRESS_4 0x20

#endif

#if IS_ENABLED(CONFIG_VIDEO_TI960)
#define TI960_LANES	4

#if IS_ENABLED(CONFIG_VIDEO_IMX390)
#define IMX390A_ADDRESS		0x44
#define IMX390B_ADDRESS		0x45
#define IMX390C_ADDRESS		0x46
#define IMX390D_ADDRESS		0x47

#define IMX390A_SER_ADDRESS	0x40
#define IMX390B_SER_ADDRESS	0x41
#define IMX390C_SER_ADDRESS	0x42
#define IMX390D_SER_ADDRESS	0x43

static struct ti960_subdev_pdata imx390_d3rcm_pdata_stub = {
	.lanes = 4,
	.gpio_powerup_seq = {0, 0xa, -1, -1},
	.module_flags = TI960_FL_POWERUP | TI960_FL_INIT_SER_CLK,
	.module_name = "imx390",
	.fsin = 0, /* gpio 0 used for FSIN */
};

static struct ti960_subdev_pdata imx390_d3cm_pdata_stub = {
	.lanes = 4,
	.gpio_powerup_seq = {0, 0x9, -1, -1},
	.module_flags = TI960_FL_POWERUP | TI960_FL_INIT_SER_CLK,
	.module_name = "imx390",
	.fsin = 3, /* gpio 3 used for FSIN */
};
#endif

#if IS_ENABLED(CONFIG_VIDEO_ISX031)

#define ISX031A_ADDRESS		0x44
#define ISX031B_ADDRESS		0x45
#define ISX031C_ADDRESS		0x46
#define ISX031D_ADDRESS		0x47

#define ISX031A_SER_ADDRESS	0x40
#define ISX031B_SER_ADDRESS	0x41
#define ISX031C_SER_ADDRESS	0x42
#define ISX031D_SER_ADDRESS	0x43

static struct ti960_subdev_pdata isx031_pdata_stub = {
  .lanes = 4,
  .fsin = 2,
  .gpio_powerup_seq = {0x00, 0x08, 0x08, -1},
  .module_flags = TI960_FL_POWERUP,
  .module_name = "isx031",
};
#endif

static struct ipu_isys_csi2_config ti960_csi2_cfg_1 = {
	.nlanes = TI960_LANES,
	.port = 1,
};

static struct ipu_isys_csi2_config ti960_csi2_cfg_2 = {
	.nlanes = TI960_LANES,
	.port = 2,
};

static struct ti960_subdev_info ti960_subdevs_1[] = {
#if IS_ENABLED(CONFIG_VIDEO_ISX031)
	{
		.board_info = {
			.type = "isx031",
			.addr = ISX031A_ADDRESS,
			.platform_data = &isx031_pdata_stub,
		},
		.rx_port = 0,
		.phy_i2c_addr = ISX031_I2C_ADDRESS,
		.ser_alias = ISX031A_SER_ADDRESS,
		.suffix = 'a',
	},
	{
		.board_info = {
			.type = "isx031",
			.addr = ISX031B_ADDRESS,
			.platform_data = &isx031_pdata_stub,
		},
		.rx_port = 1,
		.phy_i2c_addr = ISX031_I2C_ADDRESS,
		.ser_alias = ISX031B_SER_ADDRESS,
		.suffix = 'b',
	},
	{
		.board_info = {
			.type = "isx031",
			.addr = ISX031C_ADDRESS,
			.platform_data = &isx031_pdata_stub,
		},
		.rx_port = 2,
		.phy_i2c_addr = ISX031_I2C_ADDRESS,
		.ser_alias = ISX031C_SER_ADDRESS,
		.suffix = 'c',
	},
	{
		.board_info = {
			.type = "isx031",
			.addr = ISX031D_ADDRESS,
			.platform_data = &isx031_pdata_stub,
		},
		.rx_port = 3,
		.phy_i2c_addr = ISX031_I2C_ADDRESS,
		.ser_alias = ISX031D_SER_ADDRESS,
		.suffix = 'd',
	},
#endif
#if IS_ENABLED(CONFIG_VIDEO_IMX390)
	/* D3RCM */
	{
		.board_info = {
			.type = "imx390",
			.addr = IMX390A_ADDRESS,
			.platform_data = &imx390_d3rcm_pdata_stub,
		},
		.rx_port = 0,
		.phy_i2c_addr = IMX390_D3RCM_I2C_ADDRESS,
		.ser_alias = IMX390A_SER_ADDRESS,
		.suffix = 'a',
	},
	{
		.board_info = {
			.type = "imx390",
			.addr = IMX390B_ADDRESS,
			.platform_data = &imx390_d3rcm_pdata_stub,
		},
		.rx_port = 1,
		.phy_i2c_addr = IMX390_D3RCM_I2C_ADDRESS,
		.ser_alias = IMX390B_SER_ADDRESS,
		.suffix = 'b',
	},
	{
		.board_info = {
			.type = "imx390",
			.addr = IMX390C_ADDRESS,
			.platform_data = &imx390_d3rcm_pdata_stub,
		},
		.rx_port = 2,
		.phy_i2c_addr = IMX390_D3RCM_I2C_ADDRESS,
		.ser_alias = IMX390C_SER_ADDRESS,
		.suffix = 'c',
	},
	{
		.board_info = {
			.type = "imx390",
			.addr = IMX390D_ADDRESS,
			.platform_data = &imx390_d3rcm_pdata_stub,
		},
		.rx_port = 3,
		.phy_i2c_addr = IMX390_D3RCM_I2C_ADDRESS,
		.ser_alias = IMX390D_SER_ADDRESS,
		.suffix = 'd',
	},
	/* D3CM */
	{
		.board_info = {
			.type = "imx390",
			.addr = IMX390A_ADDRESS,
			.platform_data = &imx390_d3cm_pdata_stub,
		},
		.rx_port = 0,
		.phy_i2c_addr = IMX390_D3CM_I2C_ADDRESS,
		.ser_alias = IMX390A_SER_ADDRESS,
		.suffix = 'a',
	},
	{
		.board_info = {
			.type = "imx390",
			.addr = IMX390B_ADDRESS,
			.platform_data = &imx390_d3cm_pdata_stub,
		},
		.rx_port = 1,
		.phy_i2c_addr = IMX390_D3CM_I2C_ADDRESS,
		.ser_alias = IMX390B_SER_ADDRESS,
		.suffix = 'b',
	},
	{
		.board_info = {
			.type = "imx390",
			.addr = IMX390C_ADDRESS,
			.platform_data = &imx390_d3cm_pdata_stub,
		},
		.rx_port = 2,
		.phy_i2c_addr = IMX390_D3CM_I2C_ADDRESS,
		.ser_alias = IMX390C_SER_ADDRESS,
		.suffix = 'c',
	},
	{
		.board_info = {
			.type = "imx390",
			.addr = IMX390D_ADDRESS,
			.platform_data = &imx390_d3cm_pdata_stub,
		},
		.rx_port = 3,
		.phy_i2c_addr = IMX390_D3CM_I2C_ADDRESS,
		.ser_alias = IMX390D_SER_ADDRESS,
		.suffix = 'd',
	},
#endif
};

static struct ti960_subdev_info ti960_subdevs_2[] = {
#if IS_ENABLED(CONFIG_VIDEO_ISX031)
	{
		.board_info = {
			.type = "isx031",
			.addr = ISX031A_ADDRESS,
			.platform_data = &isx031_pdata_stub,
		},
		.rx_port = 0,
		.phy_i2c_addr = ISX031_I2C_ADDRESS,
		.ser_alias = ISX031A_SER_ADDRESS,
		.suffix = 'e',
	},
	{
		.board_info = {
			.type = "isx031",
			.addr = ISX031B_ADDRESS,
			.platform_data = &isx031_pdata_stub,
		},
		.rx_port = 1,
		.phy_i2c_addr = ISX031_I2C_ADDRESS,
		.ser_alias = ISX031B_SER_ADDRESS,
		.suffix = 'f',
	},
	{
		.board_info = {
			.type = "isx031",
			.addr = ISX031C_ADDRESS,
			.platform_data = &isx031_pdata_stub,
		},
		.rx_port = 2,
		.phy_i2c_addr = ISX031_I2C_ADDRESS,
		.ser_alias = ISX031C_SER_ADDRESS,
		.suffix = 'g',
	},
	{
		.board_info = {
			.type = "isx031",
			.addr = ISX031D_ADDRESS,
			.platform_data = &isx031_pdata_stub,
		},
		.rx_port = 3,
		.phy_i2c_addr = ISX031_I2C_ADDRESS,
		.ser_alias = ISX031D_SER_ADDRESS,
		.suffix = 'h',
	},
#endif
};

static struct ti960_pdata ti960_pdata_1 = {
	.subdev_info = ti960_subdevs_1,
	.subdev_num = ARRAY_SIZE(ti960_subdevs_1),
	.reset_gpio = 0,
	.FPD_gpio = -1,
	.suffix = 'a',
};

static struct ti960_pdata ti960_pdata_2 = {
	.subdev_info = ti960_subdevs_2,
	.subdev_num = ARRAY_SIZE(ti960_subdevs_2),
	.reset_gpio = 0,
	.FPD_gpio = -1,
	.suffix = 'b',
};

static struct ipu_isys_subdev_info ti960_sd_1 = {
	.csi2 = &ti960_csi2_cfg_1,
	.i2c = {
		.board_info = {
			 .type = "ti960",
			 .addr = TI960_I2C_ADDRESS_2,
			 .platform_data = &ti960_pdata_1,
		},
		.i2c_adapter_bdf = "0000:00:15.1",
	}
};

static struct ipu_isys_subdev_info ti960_sd_2 = {
	.csi2 = &ti960_csi2_cfg_2,
	.i2c = {
		.board_info = {
			 .type = "ti960",
			 .addr = TI960_I2C_ADDRESS_2,
			 .platform_data = &ti960_pdata_2,
		},
		.i2c_adapter_bdf = "0000:00:19.1",
	}
};
#endif

#if IS_ENABLED(CONFIG_VIDEO_LT6911UXC)
#define LT6911UXC_LANES       4
#define LT6911UXC_I2C_ADDRESS 0x2B

#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA) \
	&& IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING)
static void lt6911uxc_fixup_spdata(const void *spdata_rep, void *spdata)
{
	const struct ipu_spdata_rep *rep = spdata_rep;
	struct lt6911uxc_platform_data *platform = spdata;

	if (spdata_rep && spdata) {
		platform->port = rep->port_n;
		platform->lanes = rep->lanes;
		platform->i2c_slave_address = rep->slave_addr_n;
		platform->gpios[0] = rep->gpios[0];
		platform->irq_pin = rep->irq_pin;
		platform->irq_pin_flags = rep->irq_pin_flags;
		strcpy(platform->irq_pin_name, rep->irq_pin_name);
		platform->suffix = rep->suffix;
	}
}
#endif

static struct ipu_isys_csi2_config lt6911uxc_csi2_cfg_1 = {
	.nlanes = LT6911UXC_LANES,
	.port = 1,
};

static struct lt6911uxc_platform_data lt6911uxc_pdata_1 = {
	.port = 1,
	.lanes = LT6911UXC_LANES,
	.i2c_slave_address = LT6911UXC_I2C_ADDRESS,
	.irq_pin = -1,
	.irq_pin_name = "READY_STAT",
	.irq_pin_flags = IRQF_TRIGGER_RISING
		| IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	.suffix = 'a',
	.reset_pin = -1,
	.detect_pin = -1,
	.gpios = {-1, 0, 0, 0},
};

static struct ipu_isys_subdev_info  lt6911uxc_sd_1 = {
	.csi2 = &lt6911uxc_csi2_cfg_1,
	.i2c = {
	.board_info = {
		I2C_BOARD_INFO("lt6911uxc", LT6911UXC_I2C_ADDRESS),
		.platform_data = &lt6911uxc_pdata_1,
	},
	.i2c_adapter_bdf = "0000:00:15.1",
	},
#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA) \
	&& IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING)
	.fixup_spdata = lt6911uxc_fixup_spdata,
#endif
};

static struct ipu_isys_csi2_config lt6911uxc_csi2_cfg_2 = {
	.nlanes = LT6911UXC_LANES,
	.port = 2,
};

static struct lt6911uxc_platform_data lt6911uxc_pdata_2 = {
	.port = 2,
	.lanes = LT6911UXC_LANES,
	.i2c_slave_address = LT6911UXC_I2C_ADDRESS,
	.irq_pin = -1,
	.irq_pin_name = "READY_STAT",
	.irq_pin_flags = IRQF_TRIGGER_RISING
		| IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	.suffix = 'b',
	.reset_pin = -1,
	.detect_pin = -1,
	.gpios = {-1, 0, 0, 0},
};

static struct ipu_isys_subdev_info lt6911uxc_sd_2 = {
	.csi2 = &lt6911uxc_csi2_cfg_2,
	.i2c = {
	.board_info = {
		I2C_BOARD_INFO("lt6911uxc", LT6911UXC_I2C_ADDRESS),
		.platform_data = &lt6911uxc_pdata_2,
	},
	.i2c_adapter_bdf = "0000:00:19.1",
	},
#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA) \
	&& IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING)
	.fixup_spdata = lt6911uxc_fixup_spdata,
#endif
};
#endif

#if IS_ENABLED(CONFIG_VIDEO_LT6911UXE)
#define LT6911UXE_LANES       4
#define LT6911UXE_I2C_ADDRESS 0x2B

#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA) \
	&& IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING)
static void lt6911uxe_fixup_spdata(const void *spdata_rep, void *spdata)
{
	const struct ipu_spdata_rep *rep = spdata_rep;
	struct lt6911uxe_platform_data *platform = spdata;

	if (spdata_rep && spdata) {
		platform->port = rep->port_n;
		platform->lanes = rep->lanes;
		platform->i2c_slave_address = rep->slave_addr_n;
		platform->gpios[0] = rep->gpios[0];
		platform->irq_pin = rep->irq_pin;
		platform->irq_pin_flags = rep->irq_pin_flags;
		strcpy(platform->irq_pin_name, rep->irq_pin_name);
		platform->suffix = rep->suffix;
	}
}
#endif

static struct ipu_isys_csi2_config lt6911uxe_csi2_cfg_1 = {
	.nlanes = LT6911UXE_LANES,
	.port = 1,
};

static struct lt6911uxe_platform_data lt6911uxe_pdata_1 = {
	.port = 1,
	.lanes = LT6911UXE_LANES,
	.i2c_slave_address = LT6911UXE_I2C_ADDRESS,
	.irq_pin = -1,
	.irq_pin_name = "READY_STAT",
	.irq_pin_flags = IRQF_TRIGGER_RISING
		| IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	.suffix = 'a',
	.reset_pin = -1,
	.detect_pin = -1,
	.gpios = {-1, 0, 0, 0},
};

static struct ipu_isys_subdev_info  lt6911uxe_sd_1 = {
	.csi2 = &lt6911uxe_csi2_cfg_1,
	.i2c = {
	.board_info = {
		I2C_BOARD_INFO("lt6911uxe", LT6911UXE_I2C_ADDRESS),
		.platform_data = &lt6911uxe_pdata_1,
	},
	.i2c_adapter_bdf = "0000:00:15.1",
	},
#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA) \
	&& IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING)
	.fixup_spdata = lt6911uxe_fixup_spdata,
#endif
};

static struct ipu_isys_csi2_config lt6911uxe_csi2_cfg_2 = {
	.nlanes = LT6911UXE_LANES,
	.port = 2,
};

static struct lt6911uxe_platform_data lt6911uxe_pdata_2 = {
	.port = 2,
	.lanes = LT6911UXE_LANES,
	.i2c_slave_address = LT6911UXE_I2C_ADDRESS,
	.irq_pin = -1,
	.irq_pin_name = "READY_STAT",
	.irq_pin_flags = IRQF_TRIGGER_RISING
		| IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	.suffix = 'b',
	.reset_pin = -1,
	.detect_pin = -1,
	.gpios = {-1, 0, 0, 0},
};

static struct ipu_isys_subdev_info lt6911uxe_sd_2 = {
	.csi2 = &lt6911uxe_csi2_cfg_2,
	.i2c = {
	.board_info = {
		I2C_BOARD_INFO("lt6911uxe", LT6911UXE_I2C_ADDRESS),
		.platform_data = &lt6911uxe_pdata_2,
	},
	.i2c_adapter_bdf = "0000:00:19.1",
	},
#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA) \
	&& IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING)
	.fixup_spdata = lt6911uxe_fixup_spdata,
#endif
};
#endif

#if IS_ENABLED(CONFIG_VIDEO_D4XX)
#define D4XX_LANES       2
#define D4XX_I2C_ADDRESS_0 0x10
#define D4XX_I2C_ADDRESS_1 0x12
#define D4XX_I2C_ADDRESS_2 0x14
#define D4XX_PORT0      0
#define D4XX_PORT1      1
#define D4XX_PORT2      2
#define D4XX_PORT3      3

static struct ipu_isys_csi2_config d4xx_csi2_cfg_0 = {
	.nlanes = D4XX_LANES,
	.port = D4XX_PORT0,
};

static struct d4xx_pdata d4xx_pdata_0 = {
	.suffix = 'a',
};

static struct ipu_isys_subdev_info d4xx_sd_0 = {
	.csi2 = &d4xx_csi2_cfg_0,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO("d4xx", D4XX_I2C_ADDRESS_1),
			.platform_data = &d4xx_pdata_0,
		},
		.i2c_adapter_bdf = "0000:00:15.1",
	},
};

static struct ipu_isys_csi2_config d4xx_csi2_cfg_1 = {
	.nlanes = D4XX_LANES,
	.port = D4XX_PORT1,
};

static struct d4xx_pdata d4xx_pdata_1 = {
	.suffix = 'b',
};

static struct ipu_isys_subdev_info d4xx_sd_1 = {
	.csi2 = &d4xx_csi2_cfg_1,
	.i2c = {
	.board_info = {
		I2C_BOARD_INFO("d4xx", D4XX_I2C_ADDRESS_2),
		.platform_data = &d4xx_pdata_1,
	},
	.i2c_adapter_bdf = "0000:00:15.1",
	},
};

static struct ipu_isys_csi2_config d4xx_csi2_cfg_2 = {
	.nlanes = D4XX_LANES,
	.port = D4XX_PORT2,
};

static struct d4xx_pdata d4xx_pdata_2 = {
	.suffix = 'c',
};

static struct ipu_isys_subdev_info d4xx_sd_2 = {
	.csi2 = &d4xx_csi2_cfg_2,
	.i2c = {
		.board_info = {
			I2C_BOARD_INFO("d4xx", D4XX_I2C_ADDRESS_1),
			.platform_data = &d4xx_pdata_2,
		},
		.i2c_adapter_bdf = "0000:00:19.1",
	},
};

static struct ipu_isys_csi2_config d4xx_csi2_cfg_3 = {
	.nlanes = D4XX_LANES,
	.port = D4XX_PORT3,
};

static struct d4xx_pdata d4xx_pdata_3 = {
	.suffix = 'd',
};

static struct ipu_isys_subdev_info d4xx_sd_3 = {
	.csi2 = &d4xx_csi2_cfg_3,
	.i2c = {
	.board_info = {
		I2C_BOARD_INFO("d4xx", D4XX_I2C_ADDRESS_2),
	.platform_data = &d4xx_pdata_3,
	},
	.i2c_adapter_bdf = "0000:00:19.1",
	},
};
#endif

static struct ipu_isys_clk_mapping clk_mapping[] = {
	{ CLKDEV_INIT(NULL, NULL, NULL), NULL }
};

static struct ipu_isys_subdev_pdata pdata = {
	.subdevs = (struct ipu_isys_subdev_info *[]) {
#if IS_ENABLED(CONFIG_VIDEO_AR0234)
		&ar0234_sd_1,
		&ar0234_sd_2,
#endif
#if IS_ENABLED(CONFIG_VIDEO_TI960)
		&ti960_sd_1,
		&ti960_sd_2,
#endif
#if IS_ENABLED(CONFIG_VIDEO_LT6911UXC)
		&lt6911uxc_sd_1,
		&lt6911uxc_sd_2,
#endif
#if IS_ENABLED(CONFIG_VIDEO_LT6911UXE)
		&lt6911uxe_sd_1,
		&lt6911uxe_sd_2,
#endif
#if IS_ENABLED(CONFIG_VIDEO_D4XX)
		&d4xx_sd_0,
		&d4xx_sd_1,
		&d4xx_sd_2,
		&d4xx_sd_3,
#endif
		NULL,
	},
	.clk_map = clk_mapping,
};

static void ipu6_quirk(struct pci_dev *pci_dev)
{
	dev_info(&pci_dev->dev, "%s() attach the platform data", __func__);
	pci_dev->dev.platform_data = &pdata;
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, IPU6EP_ADL_P_PCI_ID, ipu6_quirk);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, IPU6EP_ADL_N_PCI_ID, ipu6_quirk);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, IPU6EP_RPL_P_PCI_ID, ipu6_quirk);

MODULE_LICENSE("GPL");
