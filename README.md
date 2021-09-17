# ipu6-drivers

This repository supports MIPI cameras through the IPU6 on Intel Alderlake platforms. There are 4 repositories that provide the complete setup:

- https://github.com/intel/ipu6-drivers (branch:adlp_tributo) - kernel drivers for the IPU and sensors
- https://github.com/intel/ipu6-camera-hal - HAL for processing of images in userspace
- https://github.com/intel/ipu6-camera-bins - IPU firmware and proprietary image processing libraries
- https://github.com/intel/icamerasrc (branch:icamerasrc_slim_api) - Gstreamer src plugin

## Content of this repository:

- IPU6 kernel driver
- Sensor dirvers OV01A10

## Build instructions:
Three ways are available:

- building with kernel source tree
- building out of kernel source tree
- and building with dkms

## build with kernel source tree

- Tested with kernel 5.13 rc5
- Check out kernel
- Copy repo content to kernel source
- Modify related Kconfig and Makefile
- Add config in LinuxRoot/drivers/media/i2c/Kconfig
```
config VIDEO_OV01A10
        tristate "OmniVision OV01A1S sensor support"
        depends on POWER_CTRL_LOGIC
        depends on VIDEO_V4L2 && I2C
        depends on ACPI || COMPILE_TEST
        select MEDIA_CONTROLLER
        select VIDEO_V4L2_SUBDEV_API
        select V4L2_FWNODE
        help
          This is a Video4Linux2 sensor driver for the OmniVision
          OV01A10 camera.

          To compile this driver as a module, choose M here: the
          module will be called ov01a10.
```
- add to drivers/media/i2c/Makefile
```makefile
obj-$(CONFIG_VIDEO_OV01A10) += ov01a10.o
```
- modify drivers/media/pci/Kconfig

replace line:
```
source "drivers/media/pci/intel/ipu3/Kconfig"
```
with:
```
source "drivers/media/pci/intel/Kconfig"
```
- Enable the following settings in .config
```
CONFIG_VIDEO_INTEL_IPU6=m
CONFIG_VIDEO_OV01A10=m
```
CVF part as below, refer to https://github.com/intel/ivsc-driver/blob/main/README.md
```
CONFIG_MFD_LJCA=m
CONFIG_I2C_LJCA=m
CONFIG_SPI_LJCA=m
CONFIG_GPIO_LJCA=m
CONFIG_INTEL_MEI_VSC=m
CONFIG_INTEL_VSC=m
CONFIG_INTEL_VSC_CSI=m
CONFIG_INTEL_VSC_ACE=m
CONFIG_INTEL_VSC_PSE=m
CONFIG_INTEL_VSC_ACE_DEBUG=m
```

### build outside kernel source tree
requires 5.13 rc5 kernel header installed on compiling machine
- to compile
```bash
$cd ipu6-drivers
$make -j8
```
- to install and use modules
```bash
$sudo make modules_install
$sudo depmod -a
```
### build with dkms
a dkms.conf file is also provided as an example for building with dkms which can be used by `dkms` `add`, `build`, `install`

