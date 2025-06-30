/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023-2025 Intel Corporation */

#include <linux/interrupt.h>
#include <media/ipu-acpi.h>
#if IS_ENABLED(CONFIG_VIDEO_ISX031)
#include <media/i2c/isx031.h>
#endif

#define CL_EMPTY 0
#define CL_DISCRETE 1
#define CL_LT 5
#define SERDES_MAX_PORT 4
#define SERDES_MAX_GPIO_POWERUP_SEQ 4
#define LOOP_SIZE 10

int get_sensor_pdata(struct device *dev,
			struct ipu_camera_module_data *data,
			void *priv, size_t size,
			enum connection_type connect,
			const char *sensor_name,
			const char *serdes_name,
			const char *hid_name);

struct ipu_isys_subdev_pdata *get_acpi_subdev_pdata(void);

struct sensor_platform_data {
	unsigned int port;
	unsigned int lanes;
	uint32_t i2c_slave_address;
	int irq_pin;
	unsigned int irq_pin_flags;
	char irq_pin_name[IPU_SPDATA_IRQ_PIN_NAME_LEN];
	int reset_pin;
	int detect_pin;
	char suffix;
	int gpios[IPU_SPDATA_GPIO_NUM];
};

struct serdes_platform_data {
	unsigned int subdev_num;
	struct serdes_subdev_info *subdev_info;
	unsigned int reset_gpio;
	unsigned int FPD_gpio;
	char suffix;
	unsigned int link_freq_mbps;
	unsigned int deser_nlanes;
	unsigned int ser_nlanes;
	struct i2c_board_info *deser_board_info;
};

struct serdes_subdev_info {
	struct i2c_board_info board_info;
	int i2c_adapter_id;
	unsigned short rx_port;
	unsigned short phy_i2c_addr;
	unsigned short ser_alias;
	char suffix[5]; /* suffix for subdevs */
};

struct serdes_module_pdata {
	unsigned short i2c_addr;
	unsigned short i2c_adapter;
	unsigned int lanes;
	int xshutdown;
	int fsin;
	int reset;
	char gpio_powerup_seq[SERDES_MAX_GPIO_POWERUP_SEQ];
	unsigned int module_flags;
	char module_name[I2C_NAME_SIZE];
	char suffix;
};

struct serdes_local {
	/* num of camera sensor connected to current mipi port */
	unsigned int rx_port;

	/* num of i2c addr for current ACPI device */
	unsigned int i2c_num;

	/* current sensor_addr */
	unsigned short sensor_addr;

	/* physical i2c addr */
	unsigned short phy_i2c_addr;

	/* last mapped addr */
	unsigned short sensor_map_addr;

	/* current serializer_addr */
	unsigned short ser_addr;

	/* last mapped addr */
	unsigned short ser_map_addr;

	/* 2nd group of mapped addr for 2x sensors */
	unsigned short sensor_map_addr_2;
	unsigned short ser_map_addr_2;

	/* current gpio_powerup_seq */
	unsigned int gpio_powerup_seq;

	/* current module flag */
	unsigned int module_flags;

	/* counter for total camera sensor connected */
	unsigned int sensor_num;

	/* counter for total deser connected */
	unsigned int deser_num;
};
