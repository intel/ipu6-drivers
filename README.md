# Intel IPU driver

There are 4 repositories:

- https://github.com/intel/ipu6-drivers/tree/iotg_ipu6 - kernel drivers for the IPU and sensors
- https://github.com/intel/ipu6-camera-bins/tree/iotg_ipu6 - IPU firmware and proprietary image processing libraries
- https://github.com/intel/ipu6-camera-hal/tree/iotg_ipu6 - HAL for processing of images in userspace
- https://github.com/intel/icamerasrc/tree/icamerasrc_slim_api (branch:icamerasrc_slim_api) - Gstreamer src plugin


## Content of this repository:
- Intel IPU kernel driver
- Drivers for AR0234, LT6911UXC and D457

## Build instructions:
Three ways are available:
1. build with kernel source tree
2. build out of kernel source tree
3. build with dkms

### 1. Build with kernel source tree
- Check out kernel source tree
- Apply patches:
```sh
	patch/IOMMU-passthrough-for-intel-ipu.diff
```
- Copy repo content to kernel source **(except Makefile and {Kconfig, Makefile} under "drivers/media/i2c/", they need to be changed manually)**
- Modify related Kconfig and Makefile
- Add config in "drivers/media/i2c/Kconfig"
```conf
config VIDEO_AR0234 
  tristate "OnSemi AR0234 sensor support" 
  depends on I2C && VIDEO_V4L2_SUBDEV_API 
  depends on MEDIA_CAMERA_SUPPORT 
  help This is a Video4Linux2 sensor-level driver for the OnSemi ar0234 camera.
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
```

- Add to drivers/media/i2c/Makefile
```makefile
obj-$(CONFIG_VIDEO_AR0234) += ar0234.o 
obj-$(CONFIG_VIDEO_LT6911UXC) += lt6911uxc.o 
obj-$(CONFIG_VIDEO_D4XX) += d4xx.o
```

- Modify drivers/media/pci/Kconfig
```conf
	# replace line:
	# source "drivers/media/pci/intel/ipu3/Kconfig"
	# with line:
	source "drivers/media/pci/intel/Kconfig"
```

- Enable below settings in .config
```conf
 CONFIG_VIDEO_INTEL_IPU6=m 
 CONFIG_INTEL_SKL_INT3472=m 
 CONFIG_VIDEO_AR0234=m 
 CONFIG_VIDEO_LT6911UXC=m 
 CONFIG_VIDEO_D4XX=m 
 CONFIG_INTEL_IPU6_ACPI=m
 CONFIG_VIDEO_INTEL_IPU_SOC=y
 CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA=y

```
### 2. Build outside kernel source tree
- Requires kernel header installed on build machine

- To build and install:
```sh
	make -j`nproc` && sudo make modules_install && sudo depmod -a
```

### 3. Build with dkms
- Register, build and auto install:
	```sh
	Install kernel header src.
	sudo dkms add .
	sudo dkms build -m ipu6-drivers -v 0.0.1
	sudo dkms autoinstall ipu6-drivers/0.0.1
	```
