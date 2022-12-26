# ipu6-drivers

This repository supports MIPI cameras through the IPU6 on Intel Tiger Lake and
Alder Lake platforms. There are 4 repositories that provide the complete setup:

- https://github.com/intel/ipu6-drivers - kernel drivers for the IPU and sensors
- https://github.com/intel/ipu6-camera-bins - IPU firmware and proprietary image processing libraries
- https://github.com/intel/ipu6-camera-hal - HAL for processing of images in userspace
- https://github.com/intel/icamerasrc/tree/icamerasrc_slim_api (branch:icamerasrc_slim_api) - Gstreamer src plugin


## Content of this repository:
- IPU6 kernel driver
- Drivers for IMX390, TI960, TI953

## Build instructions:
Three ways are available:
1. build with kernel source tree
2. build out of kernel source tree
3. and build with dkms

### 1. Build with kernel source tree
- Tested with kernel 5.10.41
- Check out kernel
- Apply patches:
	```sh
	# For IPU6
	patch/*.patch
	```
- Copy repo content to kernel source **(except Makefile and drivers/media/i2c/{Kconfig,Makefile}, will change manually next)**
- Modify related Kconfig and Makefile
- Add config in LinuxRoot/drivers/media/i2c/Kconfig *(for kernel version < 5.18, use `VIDEO_V4L2` instead of `VIDEO_DEV` in `depends on` section)*
	```conf
        config VIDEO_IMX390
        depends on I2C && VIDEO_V4L2
        tristate "IMX390 Camera Driver"
        help
          This is a Video4Linux2 sensor-level driver for Sony IMX390 camera.

        config VIDEO_TI960
        tristate "TI960 driver support"
        depends on I2C && VIDEO_V4L2
        help
          This is a driver for TI960 Deserializer.
	```

- Add to drivers/media/i2c/Makefile
	```makefile
        ti960-objs      :=      ti953-ser.o ti960-des.o
        obj-$(CONFIG_VIDEO_TI960) += ti960.o
        obj-$(CONFIG_VIDEO_IMX390) += imx390.o
	```

- Modify drivers/media/pci/Kconfig
	```conf
	# replace line:
	# source "drivers/media/pci/intel/ipu3/Kconfig"
	# with line:
	source "drivers/media/pci/intel/Kconfig"
	```

- Enable the following settings in .config
	```conf
        CONFIG_VIDEO_INTEL_IPU_SOC=y
	CONFIG_VIDEO_INTEL_IPU6=m
        CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA=y
        CONFIG_INTEL_IPU6_TGLRVP_PDATA=y
        CONFIG_VIDEO_TI960=m
        CONFIG_VIDEO_IMX390=m
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
	sudo dkms add .
	sudo dkms autoinstall ipu6-drivers/0.0.0
	```
