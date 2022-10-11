ipu6-drivers
This repository supports MIPI cameras through the IPU6 on Intel Tiger Lake and Alder Lake platforms. There are 4 repositories that provide the complete setup:

https://github.com/intel/ipu6-drivers/tree/iotg_ipu6 - kernel drivers for the IPU and sensors
https://github.com/intel/ipu6-camera-bins/tree/iotg_ipu6 - IPU firmware and proprietary image processing libraries
https://github.com/intel/ipu6-camera-hal/tree/iotg_ipu6 - HAL for processing of images in userspace
https://github.com/intel/icamerasrc/tree/icamerasrc_slim_api (branch:icamerasrc_slim_api) - Gstreamer src plugin

Content of this repository:
IPU6 kernel driver
Drivers for AR0234, LT6911UXC and D457 sensors

Build instructions:
Two ways are available:

1. build with kernel source tree
2. build out of kernel source tree

1. Build with kernel source tree
Tested with kernel 5.19

a. Check out kernel source code

b. Apply patches:

 # For IPU6
 patch/IOMMU-passthrough-for-intel-ipu.diff

c. Modify i2c related Kconfig and Makefile

Add config in LinuxRoot/drivers/media/i2c/Kconfig (for kernel version < 5.18, use VIDEO_V4L2 instead of VIDEO_DEV in depends on section)

config VIDEO_AR0234
 tristate "OnSemi AR0234 sensor support"
 depends on I2C && VIDEO_V4L2_SUBDEV_API
 depends on MEDIA_CAMERA_SUPPORT
 help
   This is a Video4Linux2 sensor-level driver for the OnSemi
   ar0234 camera.

   AR0234 is a 2Mp Digital image sensor with global shutter.

config VIDEO_LT6911UXC
 tristate "Lontium LT6911UXC decoder"
 depends on I2C && VIDEO_V4L2_SUBDEV_API
 help
   This is a Video4Linux2 sensor-level driver for the Lontium
   LT6911UXC HDMI to MIPI CSI-2 bridge.

   To compile this driver as a module, choose M here: the
   module will be called lt6911uxc.

config VIDEO_D4XX
 depends on I2C && VIDEO_V4L2
 tristate "D4XX Camera Driver"
 help
   This is a Video4Linux2 sensor-level driver for intel realsence camera.

Add to drivers/media/i2c/Makefile

obj-$(CONFIG_VIDEO_AR0234) += ar0234.o
obj-$(CONFIG_VIDEO_LT6911UXC) += lt6911uxc.o
obj-$(CONFIG_VIDEO_D4XX) += d4xx.o

d. Modify pci Kconfig
drivers/media/pci/Kconfig

 # replace line:
 # source "drivers/media/pci/intel/ipu3/Kconfig"
 # with below line:
 source "drivers/media/pci/intel/Kconfig"

e. Enable the following settings in .config

 CONFIG_VIDEO_INTEL_IPU6=m
 CONFIG_INTEL_SKL_INT3472=m
 CONFIG_VIDEO_AR0234=m
 CONFIG_VIDEO_LT6911UXC=m
 CONFIG_VIDEO_D4XX=m
 CONFIG_INTEL_IPU6_ACPI=y
 CONFIG_INTEL_IPU6_ADLRVP_PDATA=y
 CONFIG_INTEL_IPU6_TGLRVP_PDATA=y
 CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA=y
 CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING=y
 
2. Build outside kernel source tree
Requires kernel header installed on build machine

To build and install:
 make -j`nproc` && sudo make modules_install && sudo depmod -a
 
3. Build with dkms
   TBD
