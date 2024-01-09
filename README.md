# Intel IPU driver

There are 4 repositories:

- https://github.com/intel/ipu6-drivers/tree/iotg_ipu6 - kernel drivers for the IPU and sensors
- https://github.com/intel/ipu6-camera-bins/tree/iotg_ipu6 - IPU firmware and proprietary image processing libraries
- https://github.com/intel/ipu6-camera-hal/tree/iotg_ipu6 - HAL for processing of images in userspace
- https://github.com/intel/icamerasrc/tree/icamerasrc_slim_api (branch:icamerasrc_slim_api) - Gstreamer src plugin


## Content of this repository:
- Intel IPU kernel driver
- Drivers for AR0234, LT6911UXC, LT6911UXE, D457, TI960 and IMX390

### Build with kernel source tree
- Download and install Ubuntu image, check out BKC kernel source code and build kernel driver (by default IPU driver is included)
```sh
  # Newer platforms than ADL follow platform getting started guide (GSG)
	a.	Visit and login to rdc.intel.com
                Search keyword “ubuntu kernel overlay get started guide <platform name>” for the latest release version
                Look for guide, refer to section 2 for non RT kernel

                For example:
                Raptor Lake – P: 762654_Ubuntu_Kernel_Overlay_13thGenMobi_GSG_rev1.1
                Meteor Lake - U/H: 779460_Ubuntu_Kernel_Overlay_MeteorLake-U_H_GSG_0.51

        b.	Follow the GSG to download and install Ubuntu image, check out kernel source code and build kernel driver (including IPU driver)

  # ADL-P/ADL-PS/ADL-N Refer to 778598_ADL_N_MIPI_Setup_GSG section 5.0
```
- Update to the latest IPU kernel driver and build steps
```sh
	a.	Copy and replace all the files in  “include” and “driver/media” from github to source code checkouted above.
	b.	Apply additional kernel patches
	              For kernel 5.15, 5.19 and 6.2, needn’t apply any patches.
	              For kernel 6.3, apply patches in ‘patch_6.3_mainline/*.patch’.
	c.	Check and enable kernel config list in below
		Must be enabled:
	                CONFIG_VIDEO_INTEL_IPU6=m 
	                CONFIG_INTEL_SKL_INT3472=m
	                CONFIG_INTEL_IPU6_ACPI=m
	                CONFIG_VIDEO_INTEL_IPU_SOC=y
	                CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA=y

		Enabled by needed:
                        CONFIG_VIDEO_AR0234=m 
                        CONFIG_VIDEO_LT6911UXC=m
                        CONFIG_VIDEO_LT6911UXE=m 
                        CONFIG_VIDEO_D4XX=m
                        CONFIG_VIDEO_TI960=m
                        CONFIG_VIDEO_IMX390=m
	d.	Follow GSG to build kernel driver.

	NOTE: Build with iot LTS kernel 5.15, should apply the patches under
              "ipu6-drivers/kernel_patches/patch_5.15_x_iot/".
```
### Build with dkms
```sh
	Install kernel header src.
	sudo dkms add .
	sudo dkms build -m ipu6-drivers -v 0.0.1
	sudo dkms autoinstall ipu6-drivers/0.0.1
```
