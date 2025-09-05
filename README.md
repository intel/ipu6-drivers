# ipu6-drivers

This repository supports MIPI cameras through the IPU6 on Intel Tiger Lake, Alder Lake, Raptor Lake and Meteor Lake platforms.
There are 4 repositories that provide the complete setup:

- https://github.com/intel/ipu6-drivers - kernel drivers for the IPU and sensors
- https://github.com/intel/ipu6-camera-bins - IPU firmware and proprietary image processing libraries
- https://github.com/intel/ipu6-camera-hal - HAL for processing of images in userspace
- https://github.com/intel/icamerasrc/tree/icamerasrc_slim_api (branch:icamerasrc_slim_api) - Gstreamer src plugin


## Content of this repository:
- IPU6 kernel driver
- Kernel patches needed
- Drivers for HM11B1, OV01A1S, OV01A10, OV02C10, OV02E10, OV2740, HM2170, HM2172 and HI556 sensors

## Dependencies
- intel-vsc driver and LJCA USB driver (use https://github.com/intel/ivsc-driver.git for kernel version < 6.6)
- intel USB-IO driver (https://github.com/intel/usbio-drivers.git)
- INTEL_SKL_INT3472 should be enabled

## Build instructions:
Three ways are available:
1. build with kernel source tree
2. build out of kernel source tree
3. and build with dkms

### 1. Build with kernel source tree
- Tested with kernel v6.10
- Check out kernel
- Apply patches (please check detail comments below):
	```sh
	# For Meteor Lake B stepping only
	patch/0002-iommu-Add-passthrough-for-MTL-IPU.patch

	# For v5.15 <= kernel version < v5.17 and using INT3472
	patch/int3472-v5.15/*.patch

	# For v5.17 <= kernel version < v6.1.7 and using INT3472
	patch/int3472-v5.17/*.patch

	# For kernel version >= 6.1.7 and using INT3472
	patch/int3472-v6.1.7/*.patch

	# For kernel version >= 6.3 and using ov13b10
	patch/ov13b10-v6.3/*.patch

	# For kernel version v6.8,
	# patch/v6.8/0002-media-Add-IPU6-and-supported-sensors-config.patch
	# will change the related Kconfig & Makefile.
	patch/v6.8/*.patch

	# For kernel version v6.10+,
	# patch/<version>/in-tree-build/0001-media-ipu6-Workaround-to-build-PSYS.patch
	# will change the Makefile to build IPU6 PSYS driver, and
	# patch/<version>/in-tree-build/0002-media-i2c-Add-sensors-config.patch
	# will change the Makefile & Kconfig for I2C sensor drivers.
	patch/<version>/in-tree-build/*.patch
	patch/<version>/*.patch
	```

- Copy IPU6 drivers to kernel source:
	- For kernel < 6.10, need all IPU6 drivers:
	```sh
	cp -r drivers/media/pci/intel/ <your-kernel>/drivers/media/pci/
	cp -r include/* <your-kernel>/include/
	```
	- For kernel >= 6.10, only IPU6 PSYS driver needed:
	```sh
	# Out-Of-Tree IPU6 PSYS driver
	cp -r drivers/media/pci/intel/ipu6/psys <your-kernel>/drivers/media/pci/intel/ipu6/
	cp include/uapi/linux/ipu-psys.h <your-kernel>/include/uapi/linux/
	```

- Copy I2C sensor drivers to kernel source (depending on your need):
	```sh
	# Remove ipu6-drivers/drivers/media/i2c/{Kconfig,Makefile}
	# as corresponding files in your kernel was changed by patches before
	rm drivers/media/i2c/Kconfig drivers/media/i2c/Makefile
	cp -r drivers/media/i2c <your-kernel>/drivers/media/
	```

- Enable the following settings in .config
	```conf
	CONFIG_VIDEO_INTEL_IPU6=m
	CONFIG_IPU_ISYS_BRIDGE=y
	# For kernel >= v6.8 please use IPU_BRIDGE instead of IPU_ISYS_BRIDGE
	CONFIG_IPU_BRIDGE=m
	CONFIG_VIDEO_OV01A1S=m
	CONFIG_VIDEO_OV01A10=m
	CONFIG_VIDEO_HM11B1=m
	CONFIG_VIDEO_OV02C10=m
	CONFIG_VIDEO_OV02E10=m
	CONFIG_VIDEO_HM2170=m
	CONFIG_VIDEO_HM2172=m
	# Set this only if you use only 1 camera and don't want too many device nodes in media-ctl
	# CONFIG_IPU_SINGLE_BE_SOC_DEVICE=y
	# If your kernel < 5.15 or not set CONFIG_INTEL_SKL_INT3472, please set this
	# CONFIG_POWER_CTRL_LOGIC=m
	```
- LJCA and CVF part as below, please check details at https://github.com/intel/ivsc-driver/blob/main/README.md.
	```conf
	CONFIG_MFD_LJCA=m
	CONFIG_I2C_LJCA=m
	CONFIG_SPI_LJCA=m
	CONFIG_GPIO_LJCA=m
	CONFIG_USB_LJCA=m
	CONFIG_INTEL_MEI_VSC=m
	CONFIG_INTEL_MEI_VSC_HW=m
	CONFIG_INTEL_VSC=m
	CONFIG_INTEL_VSC_CSI=m
	CONFIG_INTEL_VSC_ACE=m
	CONFIG_INTEL_VSC_PSE=m
	CONFIG_INTEL_VSC_ACE_DEBUG=m
	```
### 2. Build outside kernel source tree
- Requires kernel header installed on build machine
- For kernel >= v6.10, need to patch this repo by ipu6-drivers/patches/*.patch (which can be automatically applied if you use DKMS build).
- For kernel >= v6.10, need to patch your kernel by patch/v6.10/*.patch.
- For kernel >= v6.8, still need to patch kernel by patch/v6.8/0004 & 0005 to make upstream iVSC driver work correctly. For kernel <= v6.6, requires iVSC out-of-tree driver be built together.
- To prepare out-of-tree iVSC driver under kernel <= v6.6:
	```sh
	cd ipu6-drivers
	git clone https://github.com/intel/ivsc-driver.git
	cp -r ivsc-driver/backport-include ivsc-driver/drivers ivsc-driver/include .
	rm -rf ivsc-driver
	```

- To build and install:
	```sh
	make -j`nproc` && sudo make modules_install && sudo depmod -a
	```

### 3. Build with dkms
- Prepare out-of-tree iVSC driver under kernel <= v6.6:
	```sh
	cd ipu6-drivers
	git clone https://github.com/intel/ivsc-driver.git
	cp -r ivsc-driver/backport-include ivsc-driver/drivers ivsc-driver/include ivsc-driver/dkms.conf .
	rm -rf ivsc-driver
	```

- Register, build and auto install:
	```sh
	sudo dkms add .
	sudo dkms autoinstall ipu6-drivers/0.0.0
	```
