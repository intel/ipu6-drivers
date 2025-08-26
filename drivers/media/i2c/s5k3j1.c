// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2021 Intel Corporation.

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
#include <asm/unaligned.h>
#else
#include <linux/unaligned.h>
#endif
#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>

#define S5K3J1_REG_VALUE_08BIT		1
#define S5K3J1_REG_VALUE_16BIT		2
#define S5K3J1_REG_VALUE_24BIT		3

#define S5K3J1_REG_MODE_SELECT		0x0100
#define S5K3J1_MODE_STANDBY		0x00
#define S5K3J1_MODE_STREAMING		0x01

#define S5K3J1_REG_SOFTWARE_RST		0x0103
#define S5K3J1_SOFTWARE_RST		0x01

/* Chip ID */
#define S5K3J1_REG_CHIP_ID		0x0000
#define S5K3J1_CHIP_ID			0x30a1

/* V_TIMING internal */
#define S5K3J1_REG_VTS			0x0340
#define S5K3J1_VTS_30FPS		0x0b28
#define S5K3J1_VTS_MAX			0x7fff

/* HBLANK control - read only */
#define S5K3J1_PPL_512MHZ		4704

/* Exposure control */
#define S5K3J1_REG_EXPOSURE		0x0202
#define S5K3J1_EXPOSURE_MIN		2
#define S5K3J1_EXPOSURE_STEP		1
#define S5K3J1_EXPOSURE_DEFAULT		0x40

/* Analog gain control */
#define S5K3J1_REG_ANALOG_GAIN		0x0204
#define S5K3J1_ANA_GAIN_MIN		0x20
#define S5K3J1_ANA_GAIN_MAX		0x07c0
#define S5K3J1_ANA_GAIN_STEP		1
#define S5K3J1_ANA_GAIN_DEFAULT		0x20

/* Digital gain control */
#define S5K3J1_REG_DGTL_GAIN		0x020e

#define S5K3J1_DGTL_GAIN_MIN		1024	     /* Min = 1 X */
#define S5K3J1_DGTL_GAIN_MAX		(4096 - 1)   /* Max = 4 X */
#define S5K3J1_DGTL_GAIN_DEFAULT	2560	     /* Default gain = 2.5 X */
#define S5K3J1_DGTL_GAIN_STEP		1	     /* Each step = 1/1024 */

/* Test Pattern Control */
#define S5K3J1_REG_TEST_PATTERN		0x0600

struct s5k3j1_reg {
	u16 address;
	u16 val;
};

struct s5k3j1_reg_list {
	u32 num_of_regs;
	const struct s5k3j1_reg *regs;
};

/* Link frequency config */
struct s5k3j1_link_freq_config {
	u32 pixels_per_line;

	/* registers for this link frequency */
	struct s5k3j1_reg_list reg_list;
};

/* Mode : resolution and related config&values */
struct s5k3j1_mode {
	/* Frame width */
	u32 width;
	/* Frame height */
	u32 height;

	/* V-timing */
	u32 vts_def;
	u32 vts_min;

	/* Index of Link frequency config to be used */
	u32 link_freq_index;
	/* Default register values */
	struct s5k3j1_reg_list reg_list;
};

/* 3976x2736 needs 1024Mbps/lane, 4 lanes */
static const struct s5k3j1_reg mipi_data_rate_1024mbps[] = {
	{0x0100, 0x0100},
	{0x6028, 0x4000},
	{0x0000, 0x0000},
	{0x0000, 0x30a1},
	{0xfcfc, 0x4000},
	{0x6010, 0x0001},
};

static const struct s5k3j1_reg mode_3976x2736_regs[] = {
	{0x6000, 0x0005},
	{0xfcfc, 0x4000},
	{0x6214, 0x7971},
	{0x6218, 0x7150},
	{0x6028, 0x2000},
	{0xfcfc, 0x2000},
	{0x90c8, 0x0000},
	{0x90ca, 0x0000},
	{0x90cc, 0x0000},
	{0x90ce, 0x0000},
	{0x90d0, 0x0549},
	{0x90d2, 0x0448},
	{0x90d4, 0x054a},
	{0x90d6, 0xc1f8},
	{0x90d8, 0xbc06},
	{0x90da, 0x101a},
	{0x90dc, 0xa1f8},
	{0x90de, 0xc006},
	{0x90e0, 0x00f0},
	{0x90e2, 0x65b8},
	{0x90e4, 0x2000},
	{0x90e6, 0x926c},
	{0x90e8, 0x2000},
	{0x90ea, 0x6640},
	{0x90ec, 0x2000},
	{0x90ee, 0xda00},
	{0x90f0, 0x2de9},
	{0x90f2, 0xf041},
	{0x90f4, 0x0646},
	{0x90f6, 0x3a48},
	{0x90f8, 0x0d46},
	{0x90fa, 0x0268},
	{0x90fc, 0x140c},
	{0x90fe, 0x97b2},
	{0x9100, 0x0022},
	{0x9102, 0x3946},
	{0x9104, 0x2046},
	{0x9106, 0x00f0},
	{0x9108, 0x79f8},
	{0x910a, 0x2946},
	{0x910c, 0x3046},
	{0x910e, 0x00f0},
	{0x9110, 0x7af8},
	{0x9112, 0x0122},
	{0x9114, 0x3946},
	{0x9116, 0x2046},
	{0x9118, 0x00f0},
	{0x911a, 0x70f8},
	{0x911c, 0x3148},
	{0x911e, 0xd0f8},
	{0x9120, 0x0c05},
	{0x9122, 0xb0f5},
	{0x9124, 0x805f},
	{0x9126, 0x09d9},
	{0x9128, 0x2f48},
	{0x912a, 0x0088},
	{0x912c, 0x0028},
	{0x912e, 0x05d0},
	{0x9130, 0x2e49},
	{0x9132, 0x0220},
	{0x9134, 0xa1f8},
	{0x9136, 0x0201},
	{0x9138, 0xa1f8},
	{0x913a, 0x1401},
	{0x913c, 0xbde8},
	{0x913e, 0xf081},
	{0x9140, 0x10b5},
	{0x9142, 0x284c},
	{0x9144, 0x0146},
	{0x9146, 0xd4f8},
	{0x9148, 0xec05},
	{0x914a, 0x04f2},
	{0x914c, 0xec54},
	{0x914e, 0x00f0},
	{0x9150, 0x5ff8},
	{0x9152, 0x2068},
	{0x9154, 0x00f0},
	{0x9156, 0x61f8},
	{0x9158, 0x00f0},
	{0x915a, 0x64f8},
	{0x915c, 0x2168},
	{0x915e, 0x0844},
	{0x9160, 0x2060},
	{0x9162, 0x10bd},
	{0x9164, 0x2de9},
	{0x9166, 0xf84f},
	{0x9168, 0x8246},
	{0x916a, 0x1d48},
	{0x916c, 0x8846},
	{0x916e, 0x1646},
	{0x9170, 0x8168},
	{0x9172, 0x9946},
	{0x9174, 0x0d0c},
	{0x9176, 0x8fb2},
	{0x9178, 0x0a9c},
	{0x917a, 0x0022},
	{0x917c, 0x3946},
	{0x917e, 0x2846},
	{0x9180, 0x00f0},
	{0x9182, 0x3cf8},
	{0x9184, 0x4b46},
	{0x9186, 0x3246},
	{0x9188, 0x4146},
	{0x918a, 0x5046},
	{0x918c, 0x0094},
	{0x918e, 0x00f0},
	{0x9190, 0x4ef8},
	{0x9192, 0x0122},
	{0x9194, 0x3946},
	{0x9196, 0x2846},
	{0x9198, 0x00f0},
	{0x919a, 0x30f8},
	{0x919c, 0x1248},
	{0x919e, 0x06eb},
	{0x91a0, 0x4801},
	{0x91a2, 0x4088},
	{0x91a4, 0x201a},
	{0x91a6, 0x401e},
	{0x91a8, 0xc880},
	{0x91aa, 0xbde8},
	{0x91ac, 0xf88f},
	{0x91ae, 0x10b5},
	{0x91b0, 0x0022},
	{0x91b2, 0xaff2},
	{0x91b4, 0xc301},
	{0x91b6, 0x0e48},
	{0x91b8, 0x00f0},
	{0x91ba, 0x3ef8},
	{0x91bc, 0x084c},
	{0x91be, 0x0022},
	{0x91c0, 0xaff2},
	{0x91c2, 0x8301},
	{0x91c4, 0x2060},
	{0x91c6, 0x0b48},
	{0x91c8, 0x00f0},
	{0x91ca, 0x36f8},
	{0x91cc, 0x0022},
	{0x91ce, 0xaff2},
	{0x91d0, 0x6b01},
	{0x91d2, 0x6060},
	{0x91d4, 0x0848},
	{0x91d6, 0x00f0},
	{0x91d8, 0x2ff8},
	{0x91da, 0xa060},
	{0x91dc, 0x10bd},
	{0x91de, 0x0000},
	{0x91e0, 0x2000},
	{0x91e2, 0x9260},
	{0x91e4, 0x2000},
	{0x91e6, 0x6640},
	{0x91e8, 0x2000},
	{0x91ea, 0xd900},
	{0x91ec, 0x4000},
	{0x91ee, 0xb000},
	{0x91f0, 0x0000},
	{0x91f2, 0xeeaf},
	{0x91f4, 0x0000},
	{0x91f6, 0xd86b},
	{0x91f8, 0x0000},
	{0x91fa, 0x01f5},
	{0x91fc, 0x49f2},
	{0x91fe, 0x417c},
	{0x9200, 0xc0f2},
	{0x9202, 0x000c},
	{0x9204, 0x6047},
	{0x9206, 0x4ef6},
	{0x9208, 0xaf6c},
	{0x920a, 0xc0f2},
	{0x920c, 0x000c},
	{0x920e, 0x6047},
	{0x9210, 0x4df2},
	{0x9212, 0x835c},
	{0x9214, 0xc0f2},
	{0x9216, 0x000c},
	{0x9218, 0x6047},
	{0x921a, 0x4df2},
	{0x921c, 0xb16c},
	{0x921e, 0xc0f2},
	{0x9220, 0x000c},
	{0x9222, 0x6047},
	{0x9224, 0x4df2},
	{0x9226, 0xc16c},
	{0x9228, 0xc0f2},
	{0x922a, 0x000c},
	{0x922c, 0x6047},
	{0x922e, 0x40f2},
	{0x9230, 0xf51c},
	{0x9232, 0xc0f2},
	{0x9234, 0x000c},
	{0x9236, 0x6047},
	{0x9238, 0x4bf6},
	{0x923a, 0x535c},
	{0x923c, 0xc0f2},
	{0x923e, 0x000c},
	{0x9240, 0x6047},
	{0x9242, 0x0000},
	{0x9244, 0x0000},
	{0x9246, 0x0000},
	{0x9248, 0x0000},
	{0x924a, 0x0000},
	{0x924c, 0x0000},
	{0x924e, 0x0000},
	{0x9250, 0x30a1},
	{0x9252, 0x01cb},
	{0x9254, 0x0000},
	{0x9256, 0x0026},
	{0xfcfc, 0x2000},
	{0x0e00, 0x0101},
	{0x0e50, 0x0100},
	{0x0e52, 0x00ff},
	{0x0e56, 0x0100},
	{0x0e5a, 0x001b},
	{0x0e5c, 0x171b},
	{0x0e5e, 0xf46e},
	{0x0ec0, 0x0101},
	{0x0ee4, 0x0101},
	{0x0ee6, 0x0004},
	{0x11b0, 0x0815},
	{0x11c2, 0x0815},
	{0x133a, 0x0101},
	{0x1342, 0x01ea},
	{0x1348, 0x041a},
	{0x13b4, 0x003c},
	{0x13c6, 0x0002},
	{0x1426, 0x0003},
	{0x142c, 0x000a},
	{0x16c6, 0x0011},
	{0x16cc, 0x0032},
	{0x2782, 0x01e0},
	{0x2784, 0x01ea},
	{0x2788, 0x03c0},
	{0x278a, 0x03b8},
	{0x27a0, 0x08c0},
	{0x27a2, 0x08c0},
	{0x27f4, 0x003c},
	{0x27f6, 0x003c},
	{0x2806, 0x0002},
	{0x2808, 0x0002},
	{0x2866, 0x0003},
	{0x2868, 0x0004},
	{0x286c, 0x001a},
	{0x286e, 0x0016},
	{0x2872, 0x001d},
	{0x2a52, 0x0260},
	{0x2a54, 0x0197},
	{0x2a58, 0x026b},
	{0x2a5a, 0x01a2},
	{0x2a5e, 0x0276},
	{0x2a60, 0x01ad},
	{0x2a64, 0x0281},
	{0x2a66, 0x01b8},
	{0x2a6a, 0x028c},
	{0x2a6c, 0x01c3},
	{0x2a70, 0x0297},
	{0x2a72, 0x01ce},
	{0x2a76, 0x02a2},
	{0x2a78, 0x01d9},
	{0x2a7c, 0x02ad},
	{0x2a7e, 0x01e4},
	{0x2a82, 0x02b8},
	{0x2a84, 0x01ef},
	{0x2a88, 0x02c3},
	{0x2a8a, 0x0297},
	{0x2a8e, 0x02ce},
	{0x2a90, 0x02a2},
	{0x2b06, 0x0053},
	{0x2b08, 0x001e},
	{0x2b0c, 0x009b},
	{0x2b0e, 0x0047},
	{0x2b12, 0x009b},
	{0x3bc2, 0x0100},
	{0x3bd4, 0x3005},
	{0x3bd6, 0x0505},
	{0x3bd8, 0x070c},
	{0x3bde, 0x0d0f},
	{0x3be0, 0x0701},
	{0x3bfa, 0x19ec},
	{0x3d2c, 0x8007},
	{0x3d2e, 0x000c},
	{0x3d32, 0x0130},
	{0x3d34, 0x06c0},
	{0x3dc2, 0x06de},
	{0x3dc4, 0x05a0},
	{0x3dc6, 0x076c},
	{0x3dc8, 0x0064},
	{0x3dca, 0x076c},
	{0x3dcc, 0x06e0},
	{0x3f6c, 0x0000},
	{0x3f92, 0x049e},
	{0x3fa4, 0x0000},
	{0x3fdc, 0x0000},
	{0x5208, 0x0100},
	{0x520a, 0x0000},
	{0x5214, 0x0066},
	{0x521e, 0x0000},
	{0x5228, 0x0066},
	{0x5232, 0x0066},
	{0x523c, 0x0000},
	{0x5246, 0x0000},
	{0x5250, 0x0014},
	{0x525a, 0x0033},
	{0x5264, 0x0000},
	{0x526e, 0x0000},
	{0x5278, 0x0000},
	{0x5282, 0x0000},
	{0x528c, 0x001e},
	{0x5296, 0x0033},
	{0x52ea, 0x0100},
	{0x52f6, 0x1000},
	{0x52f8, 0x1000},
	{0x52fa, 0x1000},
	{0x52fc, 0x1000},
	{0x52fe, 0x1000},
	{0x5300, 0x1000},
	{0x5302, 0x1000},
	{0x5304, 0x1000},
	{0x5306, 0x1000},
	{0x5308, 0x1000},
	{0x530a, 0x100c},
	{0x530c, 0x1010},
	{0x530e, 0x1014},
	{0x5310, 0x101c},
	{0x5312, 0x1028},
	{0x5394, 0x0003},
	{0x53b2, 0x0100},
	{0x53be, 0x0100},
	{0xd900, 0x0001},
	{0xd902, 0x0000},
	{0xfcfc, 0x4000},
	{0x0112, 0x0a0a},
	{0x0116, 0x2b00},
	{0x0118, 0x0000},
	{0x021e, 0x0000},
	{0x0380, 0x0001},
	{0x0384, 0x0001},
	{0x0402, 0x1010},
	{0x080e, 0x0307},
	{0x0810, 0x0805},
	{0x0812, 0x0a08},
	{0x0814, 0x1806},
	{0x0816, 0x0b00},
	{0x0b06, 0x0101},
	{0x0b84, 0x0201},
	{0xf45c, 0x00ff},
	{0xf462, 0x0015},
	{0xf464, 0x0016},
	{0xf466, 0x0013},
	{0xf46c, 0x0010},
	{0xf48a, 0x0020},
	{0xfcfc, 0x4000},
	{0x6000, 0x0005},
	{0xfcfc, 0x2000},
	{0x0d9c, 0x000e},
	{0x0e00, 0x0101},
	{0x0e50, 0x0100},
	{0x0e52, 0x00ff},
	{0x0e56, 0x0100},
	{0x0e5a, 0x001b},
	{0x0e5c, 0x171b},
	{0x0e5e, 0xf46e},
	{0x0ec0, 0x0101},
	{0x0ee2, 0x0030},
	{0x0ee4, 0x0101},
	{0x0ee6, 0x0004},
	{0x11b0, 0x0815},
	{0x11c2, 0x0815},
	{0x1334, 0x0e01},
	{0x1336, 0x058d},
	{0x133a, 0x0101},
	{0x1342, 0x01ea},
	{0x1348, 0x041a},
	{0x13b4, 0x003c},
	{0x13c6, 0x0002},
	{0x1426, 0x0003},
	{0x142c, 0x000a},
	{0x16c6, 0x0011},
	{0x16cc, 0x0032},
	{0x2782, 0x01e0},
	{0x2784, 0x01ea},
	{0x2788, 0x03c0},
	{0x278a, 0x03b8},
	{0x27a0, 0x08c0},
	{0x27a2, 0x08c0},
	{0x27f4, 0x003c},
	{0x27f6, 0x003c},
	{0x2806, 0x0002},
	{0x2808, 0x0002},
	{0x2866, 0x0003},
	{0x2868, 0x0004},
	{0x286c, 0x001a},
	{0x286e, 0x0016},
	{0x2872, 0x001d},
	{0x2874, 0x0017},
	{0x2a52, 0x0260},
	{0x2a54, 0x0197},
	{0x2a58, 0x026b},
	{0x2a5a, 0x01a2},
	{0x2a5e, 0x0276},
	{0x2a60, 0x01ad},
	{0x2a64, 0x0281},
	{0x2a66, 0x01b8},
	{0x2a6a, 0x028c},
	{0x2a6c, 0x01c3},
	{0x2a70, 0x0297},
	{0x2a72, 0x01ce},
	{0x2a76, 0x02a2},
	{0x2a78, 0x01d9},
	{0x2a7c, 0x02ad},
	{0x2a7e, 0x01e4},
	{0x2a82, 0x02b8},
	{0x2a84, 0x01ef},
	{0x2a88, 0x02c3},
	{0x2a8a, 0x0297},
	{0x2a8e, 0x02ce},
	{0x2a90, 0x02a2},
	{0x2b06, 0x0053},
	{0x2b08, 0x001e},
	{0x2b0c, 0x009b},
	{0x2b0e, 0x0047},
	{0x2b12, 0x009b},
	{0x2b14, 0x004e},
	{0x3bc0, 0x0300},
	{0x3bc2, 0x0100},
	{0x3bd4, 0x3005},
	{0x3bd6, 0x0505},
	{0x3bd8, 0x070c},
	{0x3bde, 0x0f0f},
	{0x3be0, 0x0701},
	{0x3be2, 0x0702},
	{0x3be8, 0x0c00},
	{0x3bea, 0x0480},
	{0x3bec, 0x4d08},
	{0x3bee, 0x0808},
	{0x3bfa, 0x19ec},
	{0x3bfc, 0x00fd},
	{0x3bfe, 0x1157},
	{0x3c00, 0x0055},
	{0x3c04, 0x0637},
	{0x3d28, 0x48aa},
	{0x3d2a, 0x4c4a},
	{0x3d2c, 0x8007},
	{0x3d2e, 0x000c},
	{0x3d32, 0x0100},
	{0x3d34, 0x06c0},
	{0x3d36, 0x0100},
	{0x3dc2, 0x06de},
	{0x3dc4, 0x05a0},
	{0x3dc6, 0x076c},
	{0x3dc8, 0x0064},
	{0x3dca, 0x076c},
	{0x3dcc, 0x06e0},
	{0x3f6c, 0x0000},
	{0x3f92, 0x049e},
	{0x3fa4, 0x0000},
	{0x3fdc, 0x0000},
	{0x42e0, 0x0200},
	{0x4c84, 0x0018},
	{0x4c86, 0x0018},
	{0x4c88, 0x0018},
	{0x4c8a, 0x0018},
	{0x4c8c, 0x0018},
	{0x4c8e, 0x0018},
	{0x4c90, 0x0018},
	{0x4c92, 0x0018},
	{0x4c94, 0x0018},
	{0x4c96, 0x0018},
	{0x4c98, 0x0018},
	{0x4c9a, 0x0018},
	{0x4c9c, 0x0018},
	{0x4c9e, 0x0018},
	{0x4ca0, 0x0018},
	{0x4ca2, 0x0018},
	{0x4fc8, 0x0000},
	{0x4fca, 0x0000},
	{0x4fcc, 0x0000},
	{0x4fce, 0x0000},
	{0x4fd0, 0x0000},
	{0x4fd2, 0x0000},
	{0x4fd4, 0x0000},
	{0x4fd6, 0x0000},
	{0x4fd8, 0x0000},
	{0x4fda, 0x0000},
	{0x4fdc, 0x0000},
	{0x4fde, 0x0000},
	{0x4fe0, 0x0000},
	{0x4fe2, 0x0000},
	{0x4fe4, 0x0000},
	{0x4fe6, 0x0000},
	{0x5208, 0x0100},
	{0x520a, 0x0000},
	{0x5214, 0x0066},
	{0x521e, 0x0000},
	{0x5228, 0x0066},
	{0x5232, 0x0066},
	{0x523c, 0x0000},
	{0x5246, 0x0000},
	{0x5250, 0x0014},
	{0x525a, 0x0033},
	{0x5264, 0x0000},
	{0x526e, 0x0000},
	{0x5278, 0x0000},
	{0x5282, 0x0000},
	{0x528c, 0x001e},
	{0x5296, 0x0033},
	{0x52ea, 0x0100},
	{0x52f6, 0x1000},
	{0x52f8, 0x1000},
	{0x52fa, 0x1000},
	{0x52fc, 0x1000},
	{0x52fe, 0x1000},
	{0x5300, 0x1000},
	{0x5302, 0x1000},
	{0x5304, 0x1000},
	{0x5306, 0x1000},
	{0x5308, 0x1000},
	{0x530a, 0x100c},
	{0x530c, 0x1010},
	{0x530e, 0x1014},
	{0x5310, 0x101c},
	{0x5312, 0x1028},
	{0x5394, 0x0003},
	{0x53b2, 0x0100},
	{0x53be, 0x0100},
	{0x5a40, 0x0100},
	{0x5a42, 0x0000},
	{0xd900, 0x0001},
	{0xd902, 0x0000},
	{0xfcfc, 0x4000},
	{0x0112, 0x0a0a},
	{0x0114, 0x0300},
	{0x0116, 0x2b00},
	{0x0118, 0x0000},
	{0x0136, 0x1333},
	{0x013e, 0x0001},
	{0x021e, 0x0000},
	{0x0300, 0x0007},
	{0x0304, 0x0002},
	{0x0306, 0x0095},
	{0x030c, 0x0000},
	{0x030e, 0x0003},
	{0x0310, 0x0109},
	{0x0312, 0x0001},
	{0x0340, 0x0b28},
	{0x0342, 0x2510},
	{0x0344, 0x0000},
	{0x0346, 0x0000},
	{0x0348, 0x1f0f},
	{0x034a, 0x0aaf},
	{0x034c, 0x0f88},
	{0x034e, 0x0ab0},
	{0x0350, 0x0000},
	{0x0352, 0x0000},
	{0x0380, 0x0001},
	{0x0382, 0x0001},
	{0x0384, 0x0001},
	{0x0386, 0x0001},
	{0x0402, 0x1010},
	{0x0404, 0x1000},
	{0x080e, 0x0407},
	{0x0810, 0x0805},
	{0x0812, 0x0a08},
	{0x0814, 0x1806},
	{0x0816, 0x0b00},
	{0x0900, 0x0221},
	{0x0b02, 0x0106},
	{0x0b04, 0x0101},
	{0x0b06, 0x0101},
	{0x0b80, 0x0000},
	{0x0b84, 0x0201},
	{0xf45c, 0x00ff},
	{0xf462, 0x0015},
	{0xf464, 0x0016},
	{0xf466, 0x0013},
	{0xf46c, 0x0010},
	{0xf48a, 0x0020},
	{0x6000, 0x0085},
	{0x6214, 0x7970},
};

static const char * const s5k3j1_test_pattern_menu[] = {
	"Disabled",
	"solid colour",
	"colour bars",
	"fade to grey colour bars",
	"PN9",
};

/* Configurations for supported link frequencies */
#define S5K3J1_LINK_FREQ_848MHZ		848000000ULL
#define S5K3J1_LINK_FREQ_INDEX_0	0

#define S5K3J1_EXT_CLK			19200000
#define S5K3J1_DATA_LANES		4

/*
 * pixel_rate = link_freq * data-rate * nr_of_lanes / bits_per_sample
 * data rate => double data rate; number of lanes => 4; bits per pixel => 10
 */
static u64 link_freq_to_pixel_rate(u64 f)
{
	f *= 2 * S5K3J1_DATA_LANES;
	do_div(f, 10);

	return f;
}

/* Menu items for LINK_FREQ V4L2 control */
static const s64 link_freq_menu_items[] = {
	S5K3J1_LINK_FREQ_848MHZ
};

/* Link frequency configs */
static const struct s5k3j1_link_freq_config
			link_freq_configs[] = {
	{
		.pixels_per_line = S5K3J1_PPL_512MHZ,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mipi_data_rate_1024mbps),
			.regs = mipi_data_rate_1024mbps,
		}
	}
};

/* Mode configs */
static const struct s5k3j1_mode supported_modes[] = {
	{
		.width = 3976,
		.height = 2736,
		.vts_def = S5K3J1_VTS_30FPS,
		.vts_min = S5K3J1_VTS_30FPS,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_3976x2736_regs),
			.regs = mode_3976x2736_regs,
		},
		.link_freq_index = S5K3J1_LINK_FREQ_INDEX_0,
	},
};

struct s5k3j1 {
	struct v4l2_subdev sd;
	struct media_pad pad;

	struct v4l2_ctrl_handler ctrl_handler;

	struct clk *img_clk;
	struct regulator *avdd;
	struct gpio_desc *reset;

	/* V4L2 Controls */
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *exposure;

	/* Current mode */
	const struct s5k3j1_mode *cur_mode;

	/* Mutex for serialized access */
	struct mutex mutex;

	/* True if the device has been identified */
	bool identified;
};

#define to_s5k3j1(_sd)	container_of(_sd, struct s5k3j1, sd)

/* Read registers up to 4 at a time */
static int s5k3j1_read_reg(struct s5k3j1 *s5k3j1,
			    u16 reg, u32 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&s5k3j1->sd);
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	int ret;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg);

	if (len > 4)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;
	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = (u8 *)&reg_addr_be;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_be_p[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = be32_to_cpu(data_be);

	return 0;
}

/* Write registers up to 4 at a time */
static int s5k3j1_write_reg(struct s5k3j1 *s5k3j1,
			     u16 reg, u32 len, u32 __val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&s5k3j1->sd);
	int buf_i, val_i;
	u8 buf[6], *val_p;
	__be32 val;

	if (len > 4)
		return -EINVAL;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	val = cpu_to_be32(__val);
	val_p = (u8 *)&val;
	buf_i = 2;
	val_i = 4 - len;

	while (val_i < 4)
		buf[buf_i++] = val_p[val_i++];

	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

/* Write a list of registers */
static int s5k3j1_write_regs(struct s5k3j1 *s5k3j1,
			      const struct s5k3j1_reg *regs, u32 len)
{
	struct i2c_client *client = v4l2_get_subdevdata(&s5k3j1->sd);
	int ret;
	u32 i;

	for (i = 0; i < len; i++) {
		ret = s5k3j1_write_reg(s5k3j1, regs[i].address, S5K3J1_REG_VALUE_16BIT,
					regs[i].val);
		if (ret) {
			dev_err_ratelimited(&client->dev,
					    "Failed to write reg 0x%4.4x. error = %d\n",
					    regs[i].address, ret);

			return ret;
		}
	}

	return 0;
}

static int s5k3j1_write_reg_list(struct s5k3j1 *s5k3j1,
				  const struct s5k3j1_reg_list *r_list)
{
	return s5k3j1_write_regs(s5k3j1, r_list->regs, r_list->num_of_regs);
}

/* Open sub-device */
static int s5k3j1_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	const struct s5k3j1_mode *default_mode = &supported_modes[0];
	struct s5k3j1 *s5k3j1 = to_s5k3j1(sd);
	struct v4l2_mbus_framefmt *try_fmt = v4l2_subdev_state_get_format(fh->state,
									  0);

	mutex_lock(&s5k3j1->mutex);

	/* Initialize try_fmt */
	try_fmt->width = default_mode->width;
	try_fmt->height = default_mode->height;
	try_fmt->code = MEDIA_BUS_FMT_SGRBG10_1X10;
	try_fmt->field = V4L2_FIELD_NONE;

	/* No crop or compose */
	mutex_unlock(&s5k3j1->mutex);

	return 0;
}

static int s5k3j1_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct s5k3j1 *s5k3j1 = container_of(ctrl->handler,
					     struct s5k3j1, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&s5k3j1->sd);
	s64 max;
	int ret;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = s5k3j1->cur_mode->height + ctrl->val - 8;
		__v4l2_ctrl_modify_range(s5k3j1->exposure,
					 s5k3j1->exposure->minimum,
					 max, s5k3j1->exposure->step, max);
		break;
	}

	/*
	 * Applying V4L2 control value only happens
	 * when power is up for streaming
	 */
	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	ret = 0;
	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		ret = s5k3j1_write_reg(s5k3j1, S5K3J1_REG_ANALOG_GAIN,
					S5K3J1_REG_VALUE_16BIT,
					ctrl->val << 1);
		break;
	case V4L2_CID_DIGITAL_GAIN:
		ret = s5k3j1_write_reg(s5k3j1, S5K3J1_REG_DGTL_GAIN,
					S5K3J1_REG_VALUE_16BIT,
					ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		ret = s5k3j1_write_reg(s5k3j1, S5K3J1_REG_EXPOSURE,
					S5K3J1_REG_VALUE_16BIT,
					ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		ret = s5k3j1_write_reg(s5k3j1, S5K3J1_REG_VTS,
					S5K3J1_REG_VALUE_16BIT,
					ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = s5k3j1_write_reg(s5k3j1, S5K3J1_REG_TEST_PATTERN,
					S5K3J1_REG_VALUE_16BIT,
					ctrl->val);
		break;
	default:
		dev_info(&client->dev,
			 "ctrl(id:0x%x,val:0x%x) is not handled\n",
			 ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops s5k3j1_ctrl_ops = {
	.s_ctrl = s5k3j1_set_ctrl,
};

static int s5k3j1_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	/* Only one bayer order(GRBG) is supported */
	if (code->index > 0)
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_SGRBG10_1X10;

	return 0;
}

static int s5k3j1_enum_frame_size(struct v4l2_subdev *sd,
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

static void s5k3j1_update_pad_format(const struct s5k3j1_mode *mode,
				      struct v4l2_subdev_format *fmt)
{
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.code = MEDIA_BUS_FMT_SGRBG10_1X10;
	fmt->format.field = V4L2_FIELD_NONE;
}

static int s5k3j1_do_get_pad_format(struct s5k3j1 *s5k3j1,
				     struct v4l2_subdev_state *sd_state,
				     struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		framefmt = v4l2_subdev_state_get_format(sd_state, fmt->pad);
		fmt->format = *framefmt;
	} else {
		s5k3j1_update_pad_format(s5k3j1->cur_mode, fmt);
	}

	return 0;
}

static int s5k3j1_get_pad_format(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_format *fmt)
{
	struct s5k3j1 *s5k3j1 = to_s5k3j1(sd);
	int ret;

	mutex_lock(&s5k3j1->mutex);
	ret = s5k3j1_do_get_pad_format(s5k3j1, sd_state, fmt);
	mutex_unlock(&s5k3j1->mutex);

	return ret;
}

static int
s5k3j1_set_pad_format(struct v4l2_subdev *sd,
		       struct v4l2_subdev_state *sd_state,
		       struct v4l2_subdev_format *fmt)
{
	struct s5k3j1 *s5k3j1 = to_s5k3j1(sd);
	const struct s5k3j1_mode *mode;
	struct v4l2_mbus_framefmt *framefmt;
	s32 vblank_def;
	s32 vblank_min;
	s64 h_blank;
	s64 pixel_rate;
	s64 link_freq;

	mutex_lock(&s5k3j1->mutex);

	/* Only one raw bayer(GRBG) order is supported */
	if (fmt->format.code != MEDIA_BUS_FMT_SGRBG10_1X10)
		fmt->format.code = MEDIA_BUS_FMT_SGRBG10_1X10;

	mode = v4l2_find_nearest_size(supported_modes,
				      ARRAY_SIZE(supported_modes),
				      width, height,
				      fmt->format.width, fmt->format.height);
	s5k3j1_update_pad_format(mode, fmt);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		framefmt = v4l2_subdev_state_get_format(sd_state, fmt->pad);
		*framefmt = fmt->format;
	} else {
		s5k3j1->cur_mode = mode;
		__v4l2_ctrl_s_ctrl(s5k3j1->link_freq, mode->link_freq_index);
		link_freq = link_freq_menu_items[mode->link_freq_index];
		pixel_rate = link_freq_to_pixel_rate(link_freq);
		__v4l2_ctrl_s_ctrl_int64(s5k3j1->pixel_rate, pixel_rate);

		/* Update limits and set FPS to default */
		vblank_def = s5k3j1->cur_mode->vts_def -
			     s5k3j1->cur_mode->height;
		vblank_min = s5k3j1->cur_mode->vts_min -
			     s5k3j1->cur_mode->height;
		__v4l2_ctrl_modify_range(s5k3j1->vblank, vblank_min,
					 S5K3J1_VTS_MAX
					 - s5k3j1->cur_mode->height,
					 1,
					 vblank_def);
		__v4l2_ctrl_s_ctrl(s5k3j1->vblank, vblank_def);
		h_blank =
			link_freq_configs[mode->link_freq_index].pixels_per_line
			 - s5k3j1->cur_mode->width;
		__v4l2_ctrl_modify_range(s5k3j1->hblank, h_blank,
					 h_blank, 1, h_blank);
	}

	mutex_unlock(&s5k3j1->mutex);

	return 0;
}

/* Verify chip ID */
static int s5k3j1_identify_module(struct s5k3j1 *s5k3j1)
{
	struct i2c_client *client = v4l2_get_subdevdata(&s5k3j1->sd);
	int ret;
	u32 val;

	if (s5k3j1->identified)
		return 0;

	ret = s5k3j1_read_reg(s5k3j1, S5K3J1_REG_CHIP_ID,
			       S5K3J1_REG_VALUE_16BIT, &val);
	if (ret)
		return ret;

	if (val != S5K3J1_CHIP_ID) {
		dev_err(&client->dev, "chip id mismatch: %x!=%x\n",
			S5K3J1_CHIP_ID, val);
		return -EIO;
	}

	s5k3j1->identified = true;

	return 0;
}

static int s5k3j1_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct s5k3j1 *s5k3j1 = to_s5k3j1(sd);

	gpiod_set_value_cansleep(s5k3j1->reset, 1);

	if (s5k3j1->avdd)
		regulator_disable(s5k3j1->avdd);

	clk_disable_unprepare(s5k3j1->img_clk);

	return 0;
}

static int s5k3j1_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct s5k3j1 *s5k3j1 = to_s5k3j1(sd);
	int ret;

	ret = clk_prepare_enable(s5k3j1->img_clk);
	if (ret < 0) {
		dev_err(dev, "failed to enable imaging clock: %d", ret);
		return ret;
	}

	if (s5k3j1->avdd) {
		ret = regulator_enable(s5k3j1->avdd);
		if (ret < 0) {
			dev_err(dev, "failed to enable avdd: %d", ret);
			clk_disable_unprepare(s5k3j1->img_clk);
			return ret;
		}
	}

	gpiod_set_value_cansleep(s5k3j1->reset, 0);
	/* 5ms to wait ready after XSHUTDN assert */
	usleep_range(5000, 5500);

	return 0;
}

static int s5k3j1_start_streaming(struct s5k3j1 *s5k3j1)
{
	struct i2c_client *client = v4l2_get_subdevdata(&s5k3j1->sd);
	const struct s5k3j1_reg_list *reg_list;
	int ret, link_freq_index;

	ret = s5k3j1_identify_module(s5k3j1);
	if (ret)
		return ret;

	/* Get out of from software reset */
	ret = s5k3j1_write_reg(s5k3j1, S5K3J1_REG_SOFTWARE_RST,
				S5K3J1_REG_VALUE_08BIT, S5K3J1_SOFTWARE_RST);
	if (ret) {
		dev_err(&client->dev, "%s failed to set powerup registers\n",
			__func__);
		return ret;
	}

	msleep(10);

	link_freq_index = s5k3j1->cur_mode->link_freq_index;
	reg_list = &link_freq_configs[link_freq_index].reg_list;
	ret = s5k3j1_write_reg_list(s5k3j1, reg_list);
	if (ret) {
		dev_err(&client->dev, "%s failed to set plls\n", __func__);
		return ret;
	}

	msleep(10);

	/* Apply default values of current mode */
	reg_list = &s5k3j1->cur_mode->reg_list;
	ret = s5k3j1_write_reg_list(s5k3j1, reg_list);
	if (ret) {
		dev_err(&client->dev, "%s failed to set mode\n", __func__);
		return ret;
	}

	/* Apply customized values from user */
	ret =  __v4l2_ctrl_handler_setup(s5k3j1->sd.ctrl_handler);
	if (ret)
		return ret;

	return s5k3j1_write_reg(s5k3j1, S5K3J1_REG_MODE_SELECT,
				 S5K3J1_REG_VALUE_08BIT,
				 S5K3J1_MODE_STREAMING);
}

/* Stop streaming */
static int s5k3j1_stop_streaming(struct s5k3j1 *s5k3j1)
{
	return s5k3j1_write_reg(s5k3j1, S5K3J1_REG_MODE_SELECT,
				 S5K3J1_REG_VALUE_08BIT, S5K3J1_MODE_STANDBY);
}

static int s5k3j1_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct s5k3j1 *s5k3j1 = to_s5k3j1(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	mutex_lock(&s5k3j1->mutex);

	if (enable) {
		ret = pm_runtime_resume_and_get(&client->dev);
		if (ret < 0)
			goto err_unlock;

		/*
		 * Apply default & customized values
		 * and then start streaming.
		 */
		ret = s5k3j1_start_streaming(s5k3j1);
		if (ret)
			goto err_rpm_put;
	} else {
		s5k3j1_stop_streaming(s5k3j1);
		pm_runtime_put(&client->dev);
	}

	mutex_unlock(&s5k3j1->mutex);

	return ret;

err_rpm_put:
	pm_runtime_put(&client->dev);
err_unlock:
	mutex_unlock(&s5k3j1->mutex);

	return ret;
}

static int s5k3j1_suspend(struct device *dev)
{
	s5k3j1_power_off(dev);

	return 0;
}

static int s5k3j1_resume(struct device *dev)
{
	return s5k3j1_power_on(dev);
}

static const struct v4l2_subdev_video_ops s5k3j1_video_ops = {
	.s_stream = s5k3j1_set_stream,
};

static const struct v4l2_subdev_pad_ops s5k3j1_pad_ops = {
	.enum_mbus_code = s5k3j1_enum_mbus_code,
	.get_fmt = s5k3j1_get_pad_format,
	.set_fmt = s5k3j1_set_pad_format,
	.enum_frame_size = s5k3j1_enum_frame_size,
};

static const struct v4l2_subdev_ops s5k3j1_subdev_ops = {
	.video = &s5k3j1_video_ops,
	.pad = &s5k3j1_pad_ops,
};

static const struct media_entity_operations s5k3j1_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_subdev_internal_ops s5k3j1_internal_ops = {
	.open = s5k3j1_open,
};

/* Initialize control handlers */
static int s5k3j1_init_controls(struct s5k3j1 *s5k3j1)
{
	struct i2c_client *client = v4l2_get_subdevdata(&s5k3j1->sd);
	struct v4l2_fwnode_device_properties props;
	struct v4l2_ctrl_handler *ctrl_hdlr;
	s64 exposure_max;
	s64 vblank_def;
	s64 vblank_min;
	s64 hblank;
	s64 pixel_rate_min;
	s64 pixel_rate_max;
	const struct s5k3j1_mode *mode;
	u32 max;
	int ret;

	ctrl_hdlr = &s5k3j1->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 10);
	if (ret)
		return ret;

	mutex_init(&s5k3j1->mutex);
	ctrl_hdlr->lock = &s5k3j1->mutex;
	max = ARRAY_SIZE(link_freq_menu_items) - 1;
	s5k3j1->link_freq = v4l2_ctrl_new_int_menu(ctrl_hdlr,
						  &s5k3j1_ctrl_ops,
						  V4L2_CID_LINK_FREQ,
						  max,
						  0,
						  link_freq_menu_items);
	if (s5k3j1->link_freq)
		s5k3j1->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	pixel_rate_max = link_freq_to_pixel_rate(link_freq_menu_items[0]);
	pixel_rate_min = 0;
	/* By default, PIXEL_RATE is read only */
	s5k3j1->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &s5k3j1_ctrl_ops,
					      V4L2_CID_PIXEL_RATE,
					      pixel_rate_min, pixel_rate_max,
					      1, pixel_rate_max);

	mode = s5k3j1->cur_mode;
	vblank_def = mode->vts_def - mode->height;
	vblank_min = mode->vts_min - mode->height;
	s5k3j1->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &s5k3j1_ctrl_ops,
					  V4L2_CID_VBLANK,
					  vblank_min,
					  S5K3J1_VTS_MAX - mode->height, 1,
					  vblank_def);

	hblank = link_freq_configs[mode->link_freq_index].pixels_per_line -
		 mode->width;
	s5k3j1->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &s5k3j1_ctrl_ops,
					  V4L2_CID_HBLANK,
					  hblank, hblank, 1, hblank);
	if (s5k3j1->hblank)
		s5k3j1->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	exposure_max = mode->vts_def - 8;
	s5k3j1->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &s5k3j1_ctrl_ops,
					    V4L2_CID_EXPOSURE,
					    S5K3J1_EXPOSURE_MIN,
					    exposure_max, S5K3J1_EXPOSURE_STEP,
					    exposure_max);

	v4l2_ctrl_new_std(ctrl_hdlr, &s5k3j1_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  S5K3J1_ANA_GAIN_MIN, S5K3J1_ANA_GAIN_MAX,
			  S5K3J1_ANA_GAIN_STEP, S5K3J1_ANA_GAIN_DEFAULT);

	/* Digital gain */
	v4l2_ctrl_new_std(ctrl_hdlr, &s5k3j1_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  S5K3J1_DGTL_GAIN_MIN, S5K3J1_DGTL_GAIN_MAX,
			  S5K3J1_DGTL_GAIN_STEP, S5K3J1_DGTL_GAIN_DEFAULT);

	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &s5k3j1_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(s5k3j1_test_pattern_menu) - 1,
				     0, 0, s5k3j1_test_pattern_menu);

	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&client->dev, "%s control init failed (%d)\n",
			__func__, ret);
		goto error;
	}

	ret = v4l2_fwnode_device_parse(&client->dev, &props);
	if (ret)
		goto error;

	ret = v4l2_ctrl_new_fwnode_properties(ctrl_hdlr, &s5k3j1_ctrl_ops,
					      &props);
	if (ret)
		goto error;

	s5k3j1->sd.ctrl_handler = ctrl_hdlr;

	return 0;

error:
	v4l2_ctrl_handler_free(ctrl_hdlr);
	mutex_destroy(&s5k3j1->mutex);

	return ret;
}

static void s5k3j1_free_controls(struct s5k3j1 *s5k3j1)
{
	v4l2_ctrl_handler_free(s5k3j1->sd.ctrl_handler);
	mutex_destroy(&s5k3j1->mutex);
}

static int s5k3j1_get_pm_resources(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct s5k3j1 *s5k3j1 = to_s5k3j1(sd);
	int ret;

	s5k3j1->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(s5k3j1->reset))
		return dev_err_probe(dev, PTR_ERR(s5k3j1->reset),
				     "failed to get reset gpio\n");

	s5k3j1->img_clk = devm_clk_get_optional(dev, NULL);
	if (IS_ERR(s5k3j1->img_clk))
		return dev_err_probe(dev, PTR_ERR(s5k3j1->img_clk),
				     "failed to get imaging clock\n");

	s5k3j1->avdd = devm_regulator_get_optional(dev, "avdd");
	if (IS_ERR(s5k3j1->avdd)) {
		ret = PTR_ERR(s5k3j1->avdd);
		s5k3j1->avdd = NULL;
		if (ret != -ENODEV)
			return dev_err_probe(dev, ret,
					     "failed to get avdd regulator\n");
	}

	return 0;
}

static int s5k3j1_check_hwcfg(struct device *dev)
{
	struct v4l2_fwnode_endpoint bus_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	struct fwnode_handle *ep;
	struct fwnode_handle *fwnode = dev_fwnode(dev);
	unsigned int i, j;
	int ret;
	u32 ext_clk;

	if (!fwnode)
		return -ENXIO;

	ep = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (!ep)
		return -EPROBE_DEFER;

	ret = fwnode_property_read_u32(dev_fwnode(dev), "clock-frequency",
				       &ext_clk);
	if (ret) {
		dev_err(dev, "can't get clock frequency");
		return ret;
	}

	if (ext_clk != S5K3J1_EXT_CLK) {
		dev_err(dev, "external clock %d is not supported",
			ext_clk);
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_alloc_parse(ep, &bus_cfg);
	fwnode_handle_put(ep);
	if (ret)
		return ret;

	if (bus_cfg.bus.mipi_csi2.num_data_lanes != S5K3J1_DATA_LANES) {
		dev_err(dev, "number of CSI2 data lanes %d is not supported",
			bus_cfg.bus.mipi_csi2.num_data_lanes);
		ret = -EINVAL;
		goto out_err;
	}

	if (!bus_cfg.nr_of_link_frequencies) {
		dev_err(dev, "no link frequencies defined");
		ret = -EINVAL;
		goto out_err;
	}

	for (i = 0; i < ARRAY_SIZE(link_freq_menu_items); i++) {
		for (j = 0; j < bus_cfg.nr_of_link_frequencies; j++) {
			if (link_freq_menu_items[i] ==
				bus_cfg.link_frequencies[j])
				break;
		}

		if (j == bus_cfg.nr_of_link_frequencies) {
			dev_err(dev, "no link frequency %lld supported",
				link_freq_menu_items[i]);
			ret = -EINVAL;
			goto out_err;
		}
	}

out_err:
	v4l2_fwnode_endpoint_free(&bus_cfg);

	return ret;
}

static int s5k3j1_probe(struct i2c_client *client)
{
	struct s5k3j1 *s5k3j1;
	bool full_power;
	int ret;

	/* Check HW config */
	ret = s5k3j1_check_hwcfg(&client->dev);
	if (ret) {
		dev_err(&client->dev, "failed to check hwcfg: %d", ret);
		return ret;
	}

	s5k3j1 = devm_kzalloc(&client->dev, sizeof(*s5k3j1), GFP_KERNEL);
	if (!s5k3j1)
		return -ENOMEM;

	/* Initialize subdev */
	v4l2_i2c_subdev_init(&s5k3j1->sd, client, &s5k3j1_subdev_ops);

	ret = s5k3j1_get_pm_resources(&client->dev);
	if (ret)
		return ret;

	full_power = acpi_dev_state_d0(&client->dev);
	if (full_power) {
		ret = s5k3j1_power_on(&client->dev);
		if (ret) {
			dev_err(&client->dev, "failed to power on\n");
			return ret;
		}

		/* Check module identity */
		ret = s5k3j1_identify_module(s5k3j1);
		if (ret) {
			dev_err(&client->dev, "failed to find sensor: %d\n", ret);
			goto error_power_off;
		}
	}

	/* Set default mode to max resolution */
	s5k3j1->cur_mode = &supported_modes[0];

	ret = s5k3j1_init_controls(s5k3j1);
	if (ret)
		goto error_power_off;

	/* Initialize subdev */
	s5k3j1->sd.internal_ops = &s5k3j1_internal_ops;
	s5k3j1->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	s5k3j1->sd.entity.ops = &s5k3j1_subdev_entity_ops;
	s5k3j1->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pad */
	s5k3j1->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&s5k3j1->sd.entity, 1, &s5k3j1->pad);
	if (ret) {
		dev_err(&client->dev, "%s failed:%d\n", __func__, ret);
		goto error_handler_free;
	}

	/*
	 * Device is already turned on by i2c-core with ACPI domain PM.
	 * Enable runtime PM and turn off the device.
	 */
	/* Set the device's state to active if it's in D0 state. */
	if (full_power)
		pm_runtime_set_active(&client->dev);
	pm_runtime_enable(&client->dev);
	pm_runtime_idle(&client->dev);

	ret = v4l2_async_register_subdev_sensor(&s5k3j1->sd);
	if (ret < 0)
		goto error_media_entity_runtime_pm;

	return 0;

error_media_entity_runtime_pm:
	pm_runtime_disable(&client->dev);
	if (full_power)
		pm_runtime_set_suspended(&client->dev);
	media_entity_cleanup(&s5k3j1->sd.entity);

error_handler_free:
	s5k3j1_free_controls(s5k3j1);
	dev_err(&client->dev, "%s failed:%d\n", __func__, ret);

error_power_off:
	s5k3j1_power_off(&client->dev);

	return ret;
}

static void s5k3j1_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k3j1 *s5k3j1 = to_s5k3j1(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	s5k3j1_free_controls(s5k3j1);

	pm_runtime_disable(&client->dev);
	pm_runtime_set_suspended(&client->dev);
}

static DEFINE_RUNTIME_DEV_PM_OPS(s5k3j1_pm_ops, s5k3j1_suspend,
				 s5k3j1_resume, NULL);

#ifdef CONFIG_ACPI
static const struct acpi_device_id s5k3j1_acpi_ids[] = {
	{"INT346D"},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(acpi, s5k3j1_acpi_ids);
#endif

static struct i2c_driver s5k3j1_i2c_driver = {
	.driver = {
		.name = "s5k3j1",
		.pm = pm_ptr(&s5k3j1_pm_ops),
		.acpi_match_table = ACPI_PTR(s5k3j1_acpi_ids),
	},
	.probe = s5k3j1_probe,
	.remove = s5k3j1_remove,
	.flags = I2C_DRV_ACPI_WAIVE_D0_PROBE,
};

module_i2c_driver(s5k3j1_i2c_driver);

MODULE_AUTHOR("Su, Jimmy <jimmy.su@intel.com>");
MODULE_DESCRIPTION("Samsung S5K3J1 sensor driver");
MODULE_LICENSE("GPL v2");
