# ipu6-drivers

This repository supports MIPI cameras through the IPU6 on Intel Tiger Lake and
Alder Lake platforms. There are 4 repositories that provide the complete setup:

- https://github.com/intel/ipu6-drivers - kernel drivers for the IPU and sensors
- https://github.com/intel/ipu6-camera-bins - IPU firmware and proprietary image processing libraries
- https://github.com/intel/ipu6-camera-hal - HAL for processing of images in userspace
- https://github.com/intel/icamerasrc/tree/icamerasrc_slim_api (branch:icamerasrc_slim_api) - Gstreamer src plugin


## Content of this repository:
- IPU6 kernel driver
- Drivers for HM11B1, OV01A1S, OV01A10, OV02C10, OV2740, HM2170 and HI556 sensors

## Build instructions:
Three ways are available:
1. build with kernel source tree
2. build out of kernel source tree
3. and build with dkms

### 1. Build with kernel source tree
- Tested with kernel 6.0
- Check out kernel
- Apply patches:
	```sh
	# For IPU6
	patch/IOMMU-passthrough-for-intel-ipu.diff

	# For 5.15 <= kernel version < 5.17
	patch/int3472-support-independent-clock-and-LED-gpios.patch

	# For kernel version >= 5.17
	patch/int3472-support-independent-clock-and-LED-gpios-5.17+.patch
	```
- Copy repo content to kernel source **(except Makefile and drivers/media/i2c/{Kconfig,Makefile}, will change manually next)**
- Modify related Kconfig and Makefile
- Add config in LinuxRoot/drivers/media/i2c/Kconfig *(for kernel version < 5.18, use `VIDEO_V4L2` instead of `VIDEO_DEV` in `depends on` section)*
	```conf
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

	config VIDEO_OV01A1S
		tristate "OmniVision OV01A1S sensor support"
		depends on VIDEO_DEV && I2C
		depends on ACPI || COMPILE_TEST
		select MEDIA_CONTROLLER
		select VIDEO_V4L2_SUBDEV_API
		select V4L2_FWNODE
		help
		  This is a Video4Linux2 sensor driver for the OmniVision
		  OV01A1S camera.

		  To compile this driver as a module, choose M here: the
		  module will be called ov01a1s.

	config VIDEO_HM11B1
		tristate "Himax HM11B1 sensor support"
		depends on VIDEO_DEV && I2C
		select MEDIA_CONTROLLER
		select VIDEO_V4L2_SUBDEV_API
		select V4L2_FWNODE
		help
		  This is a Video4Linux2 sensor driver for the Himax
		  HM11B1 camera.

		  To compile this driver as a module, choose M here: the
		  module will be called hm11b1.

	config VIDEO_OV01A10
		tristate "OmniVision OV01A10 sensor support"
		depends on VIDEO_DEV && I2C
		depends on ACPI || COMPILE_TEST
		select MEDIA_CONTROLLER
		select VIDEO_V4L2_SUBDEV_API
		select V4L2_FWNODE
		help
		  This is a Video4Linux2 sensor driver for the OmniVision
		  OV01A10 camera.

		  To compile this driver as a module, choose M here: the
		  module will be called ov01a10.

	config VIDEO_OV02C10
		tristate "OmniVision OV02C10 sensor support"
		depends on VIDEO_DEV && I2C
		depends on ACPI || COMPILE_TEST
		select MEDIA_CONTROLLER
		select VIDEO_V4L2_SUBDEV_API
		select V4L2_FWNODE
		help
		  This is a Video4Linux2 sensor driver for the OmniVision
		  OV02C10 camera.

		  To compile this driver as a module, choose M here: the
		  module will be called ov02c10.

	config VIDEO_HM2170
		tristate "Himax HM2170 sensor support"
		depends on VIDEO_DEV && I2C
		select MEDIA_CONTROLLER
		select VIDEO_V4L2_SUBDEV_API
		select V4L2_FWNODE
		help
			This is a Video4Linux2 sensor driver for the Himax
			HM2170 camera.

			To compile this driver as a module, choose M here: the
			module will be called hm2170.

	```

- Add to drivers/media/i2c/Makefile
	```makefile
	obj-$(CONFIG_POWER_CTRL_LOGIC) += power_ctrl_logic.o
	obj-$(CONFIG_VIDEO_OV01A1S) += ov01a1s.o
	obj-$(CONFIG_VIDEO_HM11B1)  += hm11b1.o
	obj-$(CONFIG_VIDEO_OV01A10) += ov01a10.o
	obj-$(CONFIG_VIDEO_OV02C10) += ov02c10.o
	obj-$(CONFIG_VIDEO_HM2170) += hm2170.o
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
	CONFIG_VIDEO_INTEL_IPU6=m
	CONFIG_IPU_ISYS_BRIDGE=y
	CONFIG_VIDEO_OV01A1S=m
	CONFIG_VIDEO_OV01A10=m
	CONFIG_VIDEO_HM11B1=m
	CONFIG_VIDEO_OV02C10=m
	CONFIG_VIDEO_HM2170=m
	# If your kernel < 5.15 or not set CONFIG_INTEL_SKL_INT3472, please add the line below:
	# CONFIG_POWER_CTRL_LOGIC=m
	```
- LJCA and CVF part as below, please check details at https://github.com/intel/ivsc-driver/blob/main/README.md
	```conf
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
### 2. Build outside kernel source tree
- Requires kernel header installed on build machine
- Requires iVSC driver be built together
- To prepare dependency:
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
- Prepare dependency:
	```sh
	cd ipu6-drivers
	git clone https://github.com/intel/ivsc-driver.git
	cp -r ivsc-driver/backport-include ivsc-driver/drivers ivsc-driver/include .
	rm -rf ivsc-driver
	```

- Register, build and auto install:
	```sh
	sudo dkms add .
	sudo dkms autoinstall ipu6-drivers/0.0.0
	```
