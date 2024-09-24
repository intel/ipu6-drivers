# ipu6-drivers

This repository supports MIPI cameras through the IPU6 on Intel Tiger Lake, Alder Lake, Raptor Lake and Meteor Lake platforms.
There are 4 repositories that provide the complete setup:

- https://github.com/intel/ipu6-drivers/tree/ia_ipu6 - kernel drivers for the IPU and sensors
- https://github.com/intel/ipu6-camera-bins/ia_ipu6 - IPU firmware and proprietary image processing libraries
- https://github.com/intel/ipu6-camera-hal/ia_ipu6 - HAL for processing of images in userspace
- https://github.com/intel/icamerasrc/tree/icamerasrc_slim_api (branch:icamerasrc_slim_api) - Gstreamer src plugin


## Content of this repository:
- IPU6 kernel driver
- Kernel patches needed
- Drivers for IMX390 and ISX031 sensors

### Build with kernel source tree
- Tested with kernel v6.1.95
- Check out kernel
- Copy `drivers` and `include` folders to kernel source **(except Kconfig & Makefile at drivers/media/pci/intel and drivers/media/i2c as they are modified by patches in previous step. You can delete them before you copy folders.)**.

- Enable the following settings in .config
	```conf
	CONFIG_VIDEO_INTEL_IPU6=m
	CONFIG_VIDEO_IMX390=m
	CONFIG_VIDEO_ISX031=m
	```
