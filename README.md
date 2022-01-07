# ipu6-drivers

This repository supports MIPI cameras through the IPU6EP on Intel Alderlake platforms. There are 4 repositories that provide the complete setup:

* https://github.com/intel/ipu6-drivers (branch:ccg_plat_adlp) - kernel drivers for the IPU and sensors
* https://github.com/intel/ipu6-camera-hal (branch:ccg_plat_adlp) - HAL for processing of images in userspace
* https://github.com/intel/ipu6-camera-bins (branch:ccg_plat_adlp) - IPU firmware and proprietary image processing libraries
* https://github.com/intel/icamerasrc (branch:icamerasrc_slim_api) - Gstreamer src plugin


## Content of this repository:
* IPU6EP kernel driver
* Driver for sensor OV8856

## Build instructions:
two ways are available:
- building with kernel source tree
- building out of kernel source tree

### build with kernel source tree
* Tested with kernel 5.15
* Check out kernel
* Copy repo content (exclude Makefile,drivers/media/i2c/Makefile) to kernel source
* Modify related Kconfig and Makefile
* Add config in LinuxRoot/drivers/media/i2c/Kconfig
```
config VIDEO_OV8856
        tristate "OmniVision OV8856 sensor support"
        depends on I2C && VIDEO_V4L2 && VIDEO_V4L2_SUBDEV_API
        depends on MEDIA_CAMERA_SUPPORT
        select V4L2_FWNODE
        help
          This is a Video4Linux2 sensor-level driver for the OmniVision
          OV8856 camera.
```

* add to drivers/media/i2c/Makefile
```
obj-$(CONFIG_VIDEO_OV8856) += ov8856.o
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

* Enable the following settings in .config
```
CONFIG_VIDEO_INTEL_IPU6=m

CONFIG_VIDEO_OV8856=m
```

### build outside kernel source tree
* requires 5.15 kernel header installed on compiling machine

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

