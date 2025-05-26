// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016--2023 Intel Corporation.
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
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 10, 0)
#include <media/ipu-isys.h>
#include "ipu.h"
#include <media/ar0234.h>
#include <media/lt6911uxc.h>
#include <media/isx031.h>
#include <media/ov2311.h>
#include <media/d4xx_pdata.h>
#else
#include "ipu6-isys.h"
#include "ipu6.h"
#endif
#include <media/ipu-acpi-pdata.h>
#include <media/ipu-acpi.h>
#include <media/imx390.h>
#include <media/ti960.h>

static LIST_HEAD(devices);

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

static const struct ipu_acpi_devices supported_devices[] = {
/*
 *	{ "ACPI ID", sensor_name, get_sensor_pdata, NULL, 0, TYPE, serdes_name },	// Custom HID
 */
	{ "INTC10C1", IMX390_NAME, get_sensor_pdata, NULL, 0, TYPE_SERDES, TI960_NAME },// IMX390 HID
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 10, 0)
	{ "INTC10CM", IMX390_NAME, get_sensor_pdata, NULL, 0, TYPE_SERDES, TI960_NAME },// new D3 IMX390 HID
	{ "INTC10C0", AR0234_NAME, get_sensor_pdata, NULL, 0, TYPE_DIRECT, NULL },	// AR0234 HID
	{ "INTC10B1", LT6911UXC_NAME, get_sensor_pdata, NULL, 0, TYPE_DIRECT, NULL },	// LT6911UXC HID
	{ "INTC1031", ISX031_NAME, get_sensor_pdata, NULL, 0, TYPE_SERDES, TI960_NAME },// ISX031 HID
	{ "INTC102R", OV2311_NAME, get_sensor_pdata, NULL, 0, TYPE_SERDES, TI960_NAME },// OV2311 HID
	{ "INTC10C5", LT6911UXE_NAME, get_sensor_pdata, NULL, 0, TYPE_DIRECT, NULL },   // LT6911UXE HID
	{ "INTC10CD", D457_NAME, get_sensor_pdata, NULL, 0, TYPE_SERDES, D457_NAME },// D457 HID
#endif
};

static int get_table_index(const char *acpi_name)
{
	unsigned int i;
	for (i = 0; i < ARRAY_SIZE(supported_devices); i++) {
		if (!strncmp(supported_devices[i].hid_name, acpi_name,
			     strlen(supported_devices[i].hid_name)))
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
	{ "INTC10B1", 0 },	// LT6911UXC HID
	{ "INTC10C1", 0 },	// IMX390 HID
	{ "INTC10CM", 0 },	// D3CMC68N-106-085 IMX390 HID
	{ "INTC1031", 0 },	// ISX031 HID
	{ "INTC102R", 0 },	// OV2311 HID
	{ "INTC10C5", 0 },	// LT6911UXE HID
	{ "INTC10CD", 0 },	// D457 HID
	{},
};

static int ipu_acpi_get_pdata(struct device *dev, int index)
{
	struct ipu_camera_module_data *camdata;
	int rval;

	if (index < 0) {
		pr_err("Device is not in supported devices list\n");
		return -ENODEV;
	}

	camdata = add_device_to_list(&devices);
	if (!camdata)
		return -ENOMEM;

	pr_info("IPU6 ACPI: Getting BIOS data for %s (%s)",
		supported_devices[index].real_driver, dev_name(dev));

	rval = supported_devices[index].get_platform_data(
		dev, camdata,
		supported_devices[index].priv_data,
		supported_devices[index].priv_size,
		supported_devices[index].connect,
		supported_devices[index].real_driver,
		supported_devices[index].serdes_name,
		supported_devices[index].hid_name);

	if (rval)
		return -EPROBE_DEFER;

	return 0;
}

/*
 * different acpi devices may have same HID, so acpi_dev_get_first_match_dev
 * will always match device to simple fwnode.
 */
static int ipu_acpi_test(struct device *dev, void *priv)
{
	struct acpi_device *adev = NULL;
	int rval;
	int acpi_idx = get_table_index(dev_name(dev));

	if (acpi_idx < 0)
		return 0;
	else
		dev_info(dev, "IPU6 ACPI: ACPI device %s\n", dev_name(dev));

	const char *target_hid = supported_devices[acpi_idx].hid_name;

	if (!ACPI_COMPANION(dev)) {
		while ((adev = acpi_dev_get_next_match_dev(adev, target_hid,
							   NULL, -1))) {
			if (adev->flags.reserved == 0) {
				adev->flags.reserved = 1;
				break;
			}
			acpi_dev_put(adev);
		}

		if (!adev) {
			dev_dbg(dev, "No ACPI device found for %s\n", target_hid);
			return 0;
		} else {
			set_primary_fwnode(dev, &adev->fwnode);
			dev_dbg(dev, "Assigned fwnode to %s\n", dev_name(dev));
		}
	}

	if (ACPI_COMPANION(dev) != adev) {
		dev_err(dev, "Failed to set ACPI companion for %s\n",
			dev_name(dev));
		acpi_dev_put(adev);
		return 0;
	}

	acpi_dev_put(adev);

	rval = ipu_acpi_get_pdata(dev, acpi_idx);
	if (rval) {
		pr_err("IPU6 ACPI: Failed to process ACPI data");
		return rval;
	}

	return 0; /* Continue iteration */
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 10, 0)
				 struct ipu_isys_csi2_config *csi2,
#else
				 struct ipu6_isys_csi2_config *csi2,
#endif
				 bool reprobe))
{
	int rval;

	if (!built_in_pdata)
		dev_dbg(dev, "Built-in pdata not found");
	else {
		dev_dbg(dev, "Built-in pdata found");
		set_built_in_pdata(*built_in_pdata);
	}

	if ((!fn) || (!driver_data))
		return -ENODEV;

	rval = acpi_bus_for_each_dev(ipu_acpi_test, NULL);
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
	set_built_in_pdata(NULL);
	return 0;
}

static void __exit ipu_acpi_exit(void)
{
}

module_init(ipu_acpi_init);
module_exit(ipu_acpi_exit);

MODULE_AUTHOR("Samu Onkalo <samu.onkalo@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IPU6 ACPI support");

