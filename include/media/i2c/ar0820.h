/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2014 - 2022 Intel Corporation */

#ifndef __AR0820_H
#define __AR0820_H

#include <linux/types.h>

#define AR0820_NAME "ar0820"

#define AR0820_I2C_ADDRESS 0x6D	// Sensing ISP I2C Address is 0xDA >> 1

struct ar0820_platform_data {
        unsigned int port;
        unsigned int lanes;
        uint32_t i2c_slave_address;
        int irq_pin;
        unsigned int irq_pin_flags;
        char irq_pin_name[16];
        char suffix;
        int gpios[4];
};

#endif /* __AR0820_H  */
