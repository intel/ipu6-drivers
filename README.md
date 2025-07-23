# Intel IPU driver

There are 4 repositories:

- https://github.com/intel/ipu6-drivers/tree/iotg_ipu6 - kernel drivers for the IPU and sensors
- https://github.com/intel/ipu6-camera-bins/tree/iotg_ipu6 - IPU firmware and proprietary image processing libraries
- https://github.com/intel/ipu6-camera-hal/tree/iotg_ipu6 - HAL for processing of images in userspace
- https://github.com/intel/icamerasrc/tree/icamerasrc_slim_api - Gstreamer src plugin


## Content of this repository:
- Intel IPU kernel driver
- Drivers for AR0234, LT6911UXC, LT6911UXE, ISX031 & MAX9X

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
                        CONFIG_VIDEO_INTEL_IPU6_ISYS_RESET=y

                For kernel version >= 6.10:
                Must be enabled:
                        CONFIG_VIDEO_INTEL_IPU6=m
                        CONFIG_IPU_BRIDGE=m
                        CONFIG_INTEL_SKL_INT3472=m
                        CONFIG_INTEL_IPU6_ACPI=m
                        CONFIG_VIDEO_INTEL_IPU6_ISYS_RESET=y

		Enabled by needed:
                        CONFIG_VIDEO_AR0234=m 
                        CONFIG_VIDEO_LT6911UXC=m
                        CONFIG_VIDEO_LT6911UXE=m
                        CONFIG_VIDEO_ISX031=m
                        CONFIG_VIDEO_MAX9X=m


        d.	Follow GSG to build kernel driver.

  # ADL-P/ADL-PS/ADL-N Refer to 778598_ADL_N_MIPI_Setup_GSG section 5.0

  NOTE: Cannot insure IPU build can work well if not based on kernel source in GSG.
        Anyway user can try community kernel source follow the readme under kernel patches folder.
        Such as 'kernel_patches/patch_6.11_mainline/README'.
```
### dkms build steps 
- Install iot Ubuntu kernel header in GSG document. 
- For kernel < v6.10: /* include: isys, psys, ar0234, lt6911uxc, lt6911uxe */
```sh
        -Rename non-upstream sensor driver code.
        $cd ipu6-drivers/drivers/media/i2c
        $mv ar0234.c.non_upstream ar0234.c
        $mv lt6911uxc.c.non_upstream lt6911uxc.c
        $mv lt6911uxe.c.non_upstream lt6911uxe.c

        $cd ipu6-drivers/
        $sudo dkms add .
	$sudo dkms build -m ipu6-drivers -v 0.0.0
	$sudo dkms autoinstall ipu6-drivers/0.0.0
```
- For kernel >= v6.10: /* include: isys, psys, ar0234, lt6911uxc, lt6911uxe, isx031 & max9x & platform (>= v6.12.15) */
        - Download the iot Ubuntu kernel source code.
        - Create dkms build source tree. /* cp isys from kernel source tree */
                a. remove isys driver for kernel < v6.10
                   $cd ipu6-drivers/drivers/media/pci/intel/ipu6/
                   $rm -rf *.h *.c Makefile
                b. copy isys driver from kernel source tree
                   $cd <kernel source tree>/drivers/media/pci/intel/ipu6
                   $cp *.c ipu6-drivers/drivers/media/pci/intel/ipu6/
                   $cp *.h ipu6-drivers/drivers/media/pci/intel/ipu6/
                   $cp Makefile ipu6-drivers/drivers/media/pci/intel/ipu6/
                   $cp Kconfig ipu6-drivers/drivers/media/pci/intel/ipu6/
```sh
                   $sudo dkms add .
                   $sudo dkms build -m ipu6-drivers -v 0.0.0
                   $sudo dkms autoinstall ipu6-drivers/0.0.0
```
