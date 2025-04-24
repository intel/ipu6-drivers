/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2025 Intel Corporation */

#ifndef __OV2311_H
#define __OV2311_H

#include <linux/types.h>

#define OV2311_NAME "ov2311"

#define OV2311_I2C_ADDRESS 0x60
#define OV2311_I2C_ADDRESS_8BIT (OV2311_I2C_ADDRESS << 1)

struct ov2311_platform_data {
	unsigned int port;
	unsigned int lanes;
	uint32_t i2c_slave_address;
	int irq_pin;
	unsigned int irq_pin_flags;
	char irq_pin_name[16];
	char suffix;
	int gpios[4];
};

#endif /* __OV2311_H  */
