# Intel IPU driver

There are 4 repositories:

- https://github.com/intel/ipu6-drivers/tree/iotg_ipu6 - kernel drivers for the IPU and sensors
- https://github.com/intel/ipu6-camera-bins/tree/iotg_ipu6 - IPU firmware and proprietary image processing libraries
- https://github.com/intel/ipu6-camera-hal/tree/iotg_ipu6 - HAL for processing of images in userspace
- https://github.com/intel/icamerasrc/tree/icamerasrc_slim_api - Gstreamer src plugin


## Content of this repository:
- Intel IPU kernel driver
- Drivers for AR0234, LT6911UXC, LT6911UXE, D457, TI960 and IMX390

### Build with kernel source tree
- Install Ubuntu image, check out kernel source code and build kernel driver
```sh
  # Newer platforms than ADL should follow platform getting started guide (GSG)
	a.	Visit and login to rdc.intel.com
                Search keyword “ubuntu kernel overlay get started guide <platform name>” for the latest release version
                Look for guide, refer to section 2 for non RT kernel

                For example:
                Raptor Lake – P GSG: 762654_Ubuntu_Kernel_Overlay_13thGenMobi_GSG_rev1.1
                Meteor Lake - U/H GSG: 779460_Ubuntu_Kernel_Overlay_MeteorLake-U_H_GSG_0.51

        b.	Follow the GSG to download and install Ubuntu image, check out kernel source code
                and build kernel driver (IPU driver included by default)

        c.	Check and enable kernel config list in below
                For kernel version < 6.10:
		Must be enabled:
	                CONFIG_VIDEO_INTEL_IPU6=m 
	                CONFIG_INTEL_SKL_INT3472=m
	                CONFIG_INTEL_IPU6_ACPI=m
	                CONFIG_VIDEO_INTEL_IPU_SOC=y
	                CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA=y

                For kernel version >= 6.10:
                Must be enabled:
                        CONFIG_VIDEO_INTEL_IPU6=m
                        CONFIG_IPU_BRIDGE=m
                        CONFIG_INTEL_SKL_INT3472=m

                For all kernel version:
		Enabled by needed:
                        CONFIG_VIDEO_INTEL_IPU6_ISYS_RESET=y
                        CONFIG_VIDEO_AR0234=m 
                        CONFIG_VIDEO_LT6911UXC=m
                        CONFIG_VIDEO_LT6911UXE=m 
                        CONFIG_VIDEO_D4XX=m
                        CONFIG_VIDEO_TI960=m
                        CONFIG_VIDEO_IMX390=m

        d.	Follow GSG to build kernel driver.

  # ADL-P/ADL-PS/ADL-N Refer to 778598_ADL_N_MIPI_Setup_GSG section 5.0

  NOTE: Cannot insure IPU build can work well if not based on kernel source in GSG.
        Anyway user can try community kernel source follow the readme under kernel patches folder.
        Such as 'kernel_patches/patch_6.11_mainline/README'.
```
### Build with dkms
- Requires kernel header installed on build machine
- For kernel >= v6.10, apply patch 'dkms_patch/0001-v6.10-IPU6-headers-used-by-PSYS.patch', refer to 'dkms.conf'.

```sh
	sudo dkms add .
	sudo dkms build -m ipu6-drivers -v 0.0.1
	sudo dkms autoinstall ipu6-drivers/0.0.1
```
