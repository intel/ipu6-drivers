# ipu6-drivers

This repository supports MIPI cameras through the IPU6 on Intel Tigerlake platforms. There are 4 repositories that provide the complete setup:

* https://github.com/intel/ipu6-drivers - kernel drivers for the IPU and sensors
* https://github.com/intel/ipu6-camera-hal - HAL for processing of images in userspace
* https://github.com/intel/ipu6-camera-bins - IPU firmware and proprietary image processing libraries
* https://github.com/intel/icamerasrc (branch:icamerasrc_slim_api) - Gstreamer src plugin


## Content of this repository:
* IPU6 kernel driver
* Drivers sensors OV01A1S and HM11B1
* Driver for LPSS USB controller

## Build instructions:
three ways are available:
- building with kernel source tree
- building out of kernel source tree
- and building with dkms

### build with kernel source tree
* Tested with kernel 5.10
* Check out kernel
* Copy repo content to kernel source
* Modify related Kconfig and Makefile
* Add config in LinuxRoot/drivers/media/i2c/Kconfig
```
config POWER_CTRL_LOGIC
        tristate "power control logic driver"
        depends on GPIO_ACPI
        help
          This is a power control logic driver for sensor, the design
          depends on camera sensor connections.
          This driver controls power by getting and using managed GPIO
          pins from ACPI config for sensors, such as HM11B1, OV01A1S.

          To compile this driver as a module, choose M here: the
          module will be called power_ctrl_logic.
```
```
config VIDEO_OV01A1S
        tristate "OmniVision OV01A1S sensor support"
        depends on POWER_CTRL_LOGIC
        depends on VIDEO_V4L2 && I2C
        depends on ACPI || COMPILE_TEST
        select MEDIA_CONTROLLER
        select VIDEO_V4L2_SUBDEV_API
        select V4L2_FWNODE
        help
          This is a Video4Linux2 sensor driver for the OmniVision
          OV01A1S camera.

          To compile this driver as a module, choose M here: the
          module will be called ov01a1s.
```
```
config VIDEO_HM11B1
       tristate "Himax HM11B1 sensor support"
       depends on POWER_CTRL_LOGIC
       depends on VIDEO_V4L2 && I2C
       select MEDIA_CONTROLLER
       select VIDEO_V4L2_SUBDEV_API
       select V4L2_FWNODE
       help
         This is a Video4Linux2 sensor driver for the Himax
         HM11B1 camera.

         To compile this driver as a module, choose M here: the
         module will be called hm11b1.
```

* add to drivers/media/i2c/Makefile
```
obj-$(CONFIG_VIDEO_OV01A1S) += ov01a1s.o
obj-$(CONFIG_VIDEO_HM11B1)  += hm11b1.o
obj-$(CONFIG_POWER_CTRL_LOGIC) += power_ctrl_logic.o
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
* Add to drivers/usb/Kconfig
```
source "drivers/usb/intel_ulpss/Kconfig"
```
* Add to drivers/usb/Makefile
```
obj-$(CONFIG_INTEL_LPSS_USB)  += intel_ulpss/
```

* Enable the following settings in .config
```
CONFIG_VIDEO_INTEL_IPU6=m

CONFIG_POWER_CTRL_LOGIC=m

CONFIG_VIDEO_OV01A1S=m
CONFIG_VIDEO_HM11B1=m

CONFIG_INTEL_LPSS_USB=m
```

### build outside kernel source tree
* requires 5.10 kernel header installed on compiling machine

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

### build with dkms
a dkms.conf file is also provided as an example for building with dkms
which can be used by `dkms` `add`,`build`,`install`
