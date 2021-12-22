# ipu6-drivers

This repository supports MIPI cameras through the IPU6SE on Intel Jasper Lake platforms. There are 4 repositories that provide the complete setup:

* https://github.com/intel/ipu6-drivers (branch:jsl_penguin_peak) - kernel drivers for the IPU and sensors
* https://github.com/intel/ipu6-camera-hal (branch:jsl_penguin_peak) - HAL for processing of images in userspace
* https://github.com/intel/ipu6-camera-bins (branch:jsl_penguin_peak) - IPU firmware and proprietary image processing libraries
* https://github.com/intel/icamerasrc (branch:icamerasrc_slim_api) - Gstreamer src plugin


## Content of this repository:
* IPU6SE kernel driver
* Driver for sensor OV13858

## Build instructions:
two ways are available:
- building with kernel source tree
- building out of kernel source tree

### build with kernel source tree
* Tested with kernel 5.10
* Check out kernel and rm exsiting ipu6 drivers
```
rm -r drivers/media/pci/intel/ipu6/

rm -r drivers/media/pci/intel/*.c
rm -r drivers/media/pci/intel/*.h
rm -r drivers/media/pci/intel/Kconfig
rm -r drivers/media/pci/intel/Makefile

rm -r include/media/ipu-isys.h
rm -r include/uapi/linux/ipu-isys.h
rm -r include/uapi/linux/ipu-psys.h
```
p.s. reason for`rm` is newer ipu6-drivers might delete files and `cp` can't propagate delete

* Copy repo content (exclude Makefile,drivers/media/i2c/Makefile) to kernel source
* Modify related Kconfig and Makefile
* Add config in LinuxRoot/drivers/media/i2c/Kconfig
```
config VIDEO_OV13858_INTEL
        tristate "OmniVision OV13858 sensor support"
        depends on I2C && VIDEO_V4L2
        select MEDIA_CONTROLLER
        select VIDEO_V4L2_SUBDEV_API
        select V4L2_FWNODE
        help
          This is a Video4Linux2 sensor driver for the OmniVision
          OV13858 camera.
```

* add to drivers/media/i2c/Makefile
```
obj-$(CONFIG_VIDEO_OV13858_INTEL) += ov13858_intel.o
```

* modify drivers/media/pci/Kconfig

replace line:
```
source "drivers/media/pci/intel/ipu3/Kconfig"
```
with line:
```
source "drivers/media/pci/intel/Kconfig"
```

* Enable/Disable the following settings in .config
```
CONFIG_VIDEO_INTEL_IPU6=m

CONFIG_VIDEO_OV13858_INTEL=m
CONFIG_VIDEO_OV13858=n
```

### build outside kernel source tree
* requires 5.10 kernel header installed on compiling machine
* requires default ov13858.ko in 5.10 not installed

to compile
```bash
$cd ipu6-drivers
$make -j8
```

to install and use modules
```bash
$sudo make modules_install
$sudo depmod -a
```

