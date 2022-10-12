/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016--2022 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/gpio-regulator.h>
#include <linux/regulator/machine.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/clkdev.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <media/ipu-isys.h>
#include "ipu.h"
#include <media/ipu-acpi-pdata.h>
#include <media/ipu-acpi.h>
#include <media/ar0234.h>
#include <media/lt6911uxc.h>
#include <media/imx390.h>
#include <media/ti960.h>
#include <media/d4xx.h>

#define HID_BUFFER_SIZE 32
#define VCM_BUFFER_SIZE 32

#define LOOP_SIZE 10
static LIST_HEAD(devices);
static LIST_HEAD(new_devs);

struct ipu_isys_subdev_pdata *ptr_built_in_pdata;

struct ipu_isys_subdev_pdata *get_built_in_pdata(void)
{
	return ptr_built_in_pdata;
}

static int get_integer_dsdt_data(struct device *dev, const u8 *dsdt,
				 int func, u64 *out)
{
	struct acpi_handle *dev_handle = ACPI_HANDLE(dev);
	union acpi_object *obj;

	obj = acpi_evaluate_dsm(dev_handle, (void *)dsdt, 0, func, NULL);
	if (!obj) {
		dev_err(dev, "No dsdt\n");
		return -ENODEV;
	}
	dev_dbg(dev, "ACPI type %d", obj->type);

	if (obj->type != ACPI_TYPE_INTEGER) {
		ACPI_FREE(obj);
		return -ENODEV;
	}
	*out = obj->integer.value;
	ACPI_FREE(obj);
	return 0;
}

static int read_acpi_block(struct device *dev, char *id, void *data, u32 size)
{
	union acpi_object *obj;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	struct acpi_handle *dev_handle = ACPI_HANDLE(dev);
	int status;
	u32 buffer_length;

	status = acpi_evaluate_object(dev_handle, id, NULL, &buffer);
	if (!ACPI_SUCCESS(status))
		return -ENODEV;

	obj = (union acpi_object *)buffer.pointer;
	if (!obj || obj->type != ACPI_TYPE_BUFFER) {
		dev_err(dev, "Could't read acpi buffer\n");
		status = -ENODEV;
		goto err;
	}

	if (obj->buffer.length > size) {
		dev_err(dev, "Given buffer is too small\n");
		status = -ENODEV;
		goto err;
	}

	memcpy(data, obj->buffer.pointer, min(size, obj->buffer.length));
	buffer_length = obj->buffer.length;
	kfree(buffer.pointer);

	return buffer_length;
err:
	kfree(buffer.pointer);
	return status;
}

static struct ipu_camera_module_data *add_device_to_list(
	struct list_head *devices)
{
	struct ipu_camera_module_data *cam_device;

	cam_device = kzalloc(sizeof(*cam_device), GFP_KERNEL);
	if (!cam_device)
		return NULL;

	list_add(&cam_device->list, devices);
	return cam_device;
}

static int ipu_acpi_get_gpio_data(struct device *dev, struct ipu_gpio_info *gpio, int size,
				u64 *gpio_num)
{
	const u8 dsdt_cam_gpio[] = {
		0x40, 0x46, 0x23, 0x79, 0x10, 0x9e, 0xea, 0x4f,
		0xa5, 0xc1, 0xB5, 0xaa, 0x8b, 0x19, 0x75, 0x6f };

	int i = 0, j = 0, retries = 0, loop = 0;
	u64 num_gpio;

	int rval = get_integer_dsdt_data(dev, dsdt_cam_gpio, 1, &num_gpio);

	if (rval < 0) {
		dev_err(dev, "Failed to get number of GPIO pins\n");
		return rval;
	}

	dev_dbg(dev, "Num of gpio found = %lld", num_gpio);

	if (num_gpio == 0) {
		*gpio_num = num_gpio;
		return rval;
	}

	if (num_gpio > size) {
		dev_err(dev, "Incorrect number of GPIO pins\n");
		return rval;
	}

	/* repeat until all gpio pin is saved */
	while (i < num_gpio && loop <= LOOP_SIZE) {
		u64 data;
		struct gpio_desc *desc = NULL;

		rval = get_integer_dsdt_data(dev, dsdt_cam_gpio, i + 2, &data);

		if (rval < 0) {
			dev_err(dev, "No gpio data\n");
			return -ENODEV;
		}

		gpio[i].func = (data & 0xff);
		gpio[i].valid = FALSE;

		desc = gpiod_get_index(dev, NULL, i + retries, GPIOD_ASIS);

		if (!IS_ERR(desc)) {
			unsigned short pin = desc_to_gpio(desc);
			bool save = TRUE;

			/* always save first GPIO pin */
			if (i == 0)
				save = TRUE;

			/* check subsequent GPIO pin for replicate */
			else {
				for (j = 0; j <= i; j++) {
					/* retry if same as previous pin */
					if (gpio[j].pin == pin) {
						retries++;
						save = FALSE;
						gpiod_put(desc);
						break;
					}
				}
			}

			/* save into array */
			if (save == TRUE) {
				gpio[i].pin = pin;
				gpio[i].valid = TRUE;
				gpiod_put(desc);
				i++;
				retries = 0;
			}
		}
		loop++;
	}
	*gpio_num = num_gpio;

	return rval;
}

static int ipu_acpi_get_i2c_info(struct device *dev, struct ipu_i2c_info *i2c, int size, u64 *num)
{
	const u8 dsdt_cam_i2c[] = {
		0x49, 0x75, 0x25, 0x26, 0x71, 0x92, 0xA4, 0x4C,
		0xBB, 0x43, 0xC4, 0x89, 0x9D, 0x5A, 0x48, 0x81};

	u64 num_i2c;
	int i;
	int rval = get_integer_dsdt_data(dev, dsdt_cam_i2c, 1, &num_i2c);

	if (rval < 0) {
		dev_err(dev, "Failed to get number of I2C\n");
		return -ENODEV;
	}

	for (i = 0; i < num_i2c && i < size; i++) {
		u64 data;

		rval = get_integer_dsdt_data(dev, dsdt_cam_i2c, i + 2,
					     &data);

		if (rval < 0) {
			dev_err(dev, "Failed to get I2C data\n");
			return -ENODEV;
		}

		i2c[i].bus = ((data >> 24) & 0xff);
		i2c[i].addr = (data >> 8) & 0xff;

		dev_dbg(dev, "ACPI camera option: i2c bus %d addr %x",
			i2c[i].bus, i2c[i].addr);
	}

	*num = num_i2c;

	return 0;
}

static int match_depend(struct device *dev, const void *data)
{
	return (dev && dev->fwnode == data) ? 1 : 0;
}

#define MAX_CONSUMERS 1
int ipu_acpi_get_control_logic_data(struct device *dev,
					struct control_logic_data **ctl_data)
{
	/* CLDB data */
	struct control_logic_data_packed ctl_logic_data;
	int ret = read_acpi_block(dev, "CLDB", &ctl_logic_data,
				sizeof(ctl_logic_data));

	if (ret < 0) {
		dev_err(dev, "no such CLDB block");
		return ret;
	}

	(*ctl_data)->type = ctl_logic_data.controllogictype;
	(*ctl_data)->id = ctl_logic_data.controllogicid;
	(*ctl_data)->sku = ctl_logic_data.sensorcardsku;

	dev_dbg(dev, "CLDB data version %d clid %d cltype %d sku %d",
		ctl_logic_data.version,
		ctl_logic_data.controllogicid,
		ctl_logic_data.controllogictype,
		ctl_logic_data.sensorcardsku);

	/* GPIO data */
	ret = ipu_acpi_get_gpio_data(dev, (*ctl_data)->gpio, ARRAY_SIZE((*ctl_data)->gpio),
				&((*ctl_data)->gpio_num));

	if (ret < 0) {
		dev_err(dev, "Failed to get GPIO data");
		return ret;
	}
	return 0;
}

int ipu_acpi_get_dep_data(struct device *dev,
			     struct control_logic_data *ctl_data)
{
	struct acpi_handle *dev_handle = ACPI_HANDLE(dev);
	struct acpi_handle_list dep_devices;
	acpi_status status;
	int i;
	int rval;

	ctl_data->completed = false;

	if (!acpi_has_method(dev_handle, "_DEP")) {
		dev_err(dev, "ACPI does not have _DEP method");
		return 0;
	}

	status = acpi_evaluate_reference(dev_handle, "_DEP", NULL,
					 &dep_devices);

	if (ACPI_FAILURE(status)) {
		dev_err(dev, "Failed to evaluate _DEP.\n");
		return -ENODEV;
	}

	for (i = 0; i < dep_devices.count; i++) {
		struct acpi_device *device;
		struct acpi_device_info *info;
		struct device *p_dev;
		int match;

		status = acpi_get_object_info(dep_devices.handles[i], &info);
		if (ACPI_FAILURE(status)) {
			dev_err(dev, "Error reading _DEP device info\n");
			continue;
		}

		match = info->valid & ACPI_VALID_HID &&
			!strcmp(info->hardware_id.string, "INT3472");

		kfree(info);

		if (!match)
			continue;

		/* Process device IN3472 created by acpi */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
		if (acpi_bus_get_device(dep_devices.handles[i], &device)) {
#else
		device = acpi_fetch_acpi_dev(dep_devices.handles[i]);
		if (!device) {
#endif
			dev_err(dev, "INT3472 does not have dep device");
			return -ENODEV;
		}

		dev_dbg(dev, "Depend ACPI device found: %s\n",
			dev_name(&device->dev));

		p_dev = bus_find_device(&platform_bus_type, NULL,
					&device->fwnode, match_depend);

		if (p_dev) {
			dev_err(dev, "Dependent platform device found %s\n",
				dev_name(p_dev));

			/* obtain Control Logic Data from BIOS */
			rval = ipu_acpi_get_control_logic_data(p_dev, &ctl_data);

			if (rval) {
				dev_err(dev, "Error getting Control Logic Data");
				return rval;
			} else
				ctl_data->completed = true;
		} else
			dev_err(dev, "Dependent platform device not found\n");
	}

	if (!ctl_data->completed) {
		ctl_data->type = CL_EMPTY;
		dev_err(dev, "No control logic data available");
	}

	return 0;
}

int ipu_acpi_get_cam_data(struct device *dev,
			     struct sensor_bios_data *sensor)
{
	/* SSDB */
	struct sensor_bios_data_packed sensor_data;

	int ret = read_acpi_block(dev, "SSDB", &sensor_data,
				  sizeof(sensor_data));

	if (ret < 0) {
		dev_err(dev, "Fail to read from SSDB");
		return ret;
	}

	/* Xshutdown is not part of the ssdb data */
	sensor->link = sensor_data.link;
	sensor->lanes = sensor_data.lanes;
	sensor->pprval = sensor_data.pprval;
	sensor->pprunit = sensor_data.pprunit;

	dev_dbg(dev, "sensor ACPI data: name %s link %d, lanes %d pprval %d pprunit %x",
		dev_name(dev), sensor->link, sensor->lanes, sensor->pprval, sensor->pprunit);

	/* I2C */
	ret = ipu_acpi_get_i2c_info(dev, sensor->i2c, ARRAY_SIZE(sensor->i2c), &sensor->i2c_num);

	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(ipu_acpi_get_cam_data);

static const struct ipu_acpi_devices supported_devices[] = {
/*
 *	{ "ACPI ID", sensor_name, get_sensor_pdata, NULL, 0, TYPE, serdes_name },	// Custom HID
 */
	{ "INTC10C0", AR0234_NAME, get_sensor_pdata, NULL, 0, TYPE_DIRECT, NULL },	// AR0234 HID
	{ "INTC10B1", LT6911UXC_NAME, get_sensor_pdata, NULL, 0, TYPE_DIRECT, NULL },	// Lontium HID
	{ "INTC10C1", IMX390_NAME, get_sensor_pdata, NULL, 0, TYPE_SERDES, TI960_NAME },// IMX390 HID
	{ "INTC10CD", D457_NAME, get_sensor_pdata, NULL, 0, TYPE_DIRECT, NULL },	// D457 HID
	/* for later usage */
//	{ "INTC10CD", D457_NAME, get_sensor_pdata, NULL, 0, TYPE_SERDES, MAX9296_NAME },// D457 HID
};

static int get_table_index(struct device *device, const __u8 *acpi_name)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_devices); i++) {
		if (!strcmp(acpi_name, supported_devices[i].hid_name))
			return i;
	}

	return -ENODEV;
}

/* List of ACPI devices what we can handle */
/* Must match with HID in BIOS option. Add new sensor if required */
static const struct acpi_device_id ipu_acpi_match[] = {
/*
 *	{ "AR0234A", 0 },	// Custom HID
 */
	{ "INTC10C0", 0 },	// AR0234 HID
	{ "INTC10B1", 0 },	// Lontium HID
	{ "INTC10C1", 0 },	// IMX390 HID
	{ "INTC10CD", 0 },	// D457 HID
	{},
};
static int ipu_acpi_get_pdata(struct i2c_client *client,
				 const struct acpi_device_id *acpi_id,
				 struct ipu_i2c_helper *helper)
{
	struct ipu_camera_module_data *camdata;
	int index = get_table_index(&client->dev, acpi_id->id);

	if (index < 0) {
		dev_err(&client->dev,
			"Device is not in supported devices list\n");
		return -ENODEV;
	}

	camdata = add_device_to_list(&devices);
	if (!camdata)
		return -ENOMEM;

	strlcpy(client->name, supported_devices[index].real_driver,
		sizeof(client->name));

	dev_info(&client->dev, "Getting BIOS data for %s", client->name);

	supported_devices[index].get_platform_data(
		client, camdata, helper,
		supported_devices[index].priv_data,
		supported_devices[index].priv_size,
		supported_devices[index].connect,
		supported_devices[index].serdes_name);

	return 0;
}

static int ipu_i2c_test(struct device *dev, void *priv)
{
	struct i2c_client *client = i2c_verify_client(dev);
	const struct acpi_device_id *acpi_id;

	/*
	 * Check that we are handling only I2C devices which really has
	 * ACPI data and are one of the devices which we want to handle
	 */

	if (!ACPI_COMPANION(dev) || !client)
		return 0;

	acpi_id = acpi_match_device(ipu_acpi_match, dev);
	if (!acpi_id) {
		dev_err(dev, "acpi id not found, return 0");
		return 0;
	}

	/*
	 * Skip if platform data has already been added.
	 * Probably ACPI data overruled by kernel platform data
	 */
	if (client->dev.platform_data)
		return 0;

	/* Looks that we got what we are looking for */
	if (ipu_acpi_get_pdata(client, acpi_id, priv))
		dev_err(dev, "Failed to process ACPI data");

	/* Don't return error since we want to process remaining devices */

	/* Unregister matching client */
	i2c_unregister_device(client);

	return 0;
}

/* Scan all i2c devices and pick ones which we can handle */

/* Try to get all IPU related devices mentioned in BIOS and all related information
 * If there is existing ipu_isys_subdev_pdata, update the existing pdata
 * If not, return a new generated existing pdata
 */

int ipu_get_acpi_devices(void *driver_data,
				struct device *dev,
				struct ipu_isys_subdev_pdata **spdata,
				struct ipu_isys_subdev_pdata **built_in_pdata,
				int (*fn)
				(struct device *, void *,
				 struct ipu_isys_csi2_config *csi2,
				 bool reprobe))
{
	struct ipu_i2c_helper helper = {
		.fn = fn,
		.driver_data = driver_data,
	};
	int rval;

	if (!built_in_pdata)
		dev_dbg(dev, "Built-in pdata not found");
	else {
		dev_dbg(dev, "Built-in pdata found");
		ptr_built_in_pdata = *built_in_pdata;
	}

	if ((!fn) || (!driver_data))
		return -ENODEV;

	rval = i2c_for_each_dev(&helper, ipu_i2c_test);
	if (rval < 0)
		return rval;

	if (!built_in_pdata) {
		dev_dbg(dev, "Return ACPI generated pdata");
		*spdata = get_acpi_subdev_pdata();
	} else
		dev_dbg(dev, "Return updated built-in pdata");

	return 0;
}
EXPORT_SYMBOL(ipu_get_acpi_devices);

static int __init ipu_acpi_init(void)
{
	ptr_built_in_pdata = NULL;
	return 0;
}

static void __exit ipu_acpi_exit(void)
{
}

module_init(ipu_acpi_init);
module_exit(ipu_acpi_exit);

MODULE_AUTHOR("Samu Onkalo <samu.onkalo@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IPU4 ACPI support");

