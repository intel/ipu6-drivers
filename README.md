# ipu6-drivers

This repository supports MIPI cameras through the IPU6EP on Intel Alder Lake, Raptor Lake and Meteor Lake platforms. There are 4 repositories that provide the complete setup:

* https://github.com/intel/ipu6-drivers/tree/ccg_plat_adlp (branch:ccg_plat_adlp) - kernel drivers for the IPU and sensors
* https://github.com/intel/ipu6-camera-bins/tree/ccg_plat_adlp (branch:ccg_plat_adlp) - IPU firmware and proprietary image processing libraries
* https://github.com/intel/ipu6-camera-hal/tree/ccg_plat_adlp (branch:ccg_plat_adlp) - HAL for processing of images in userspace
* https://github.com/intel/icamerasrc/tree/icamerasrc_slim_api (branch:icamerasrc_slim_api) - Gstreamer src plugin

## Content of this repository:
* IPU6EP kernel driver
* Driver for sensor OV8856
* Patch on kernel source (essential to enable IPU and OV13B10)

## Build instructions:
two ways are available:
- building with kernel source tree
- building out of kernel source tree

### Build with kernel source tree
* Tested with kernel 6.0
* Check out kernel
* Apply `patch.diff` to kernel source code
* Copy repo content (exclude Makefile,drivers/media/i2c/Makefile) to kernel source
* Modify drivers/media/pci/Kconfig

```
# replace line:
# source "drivers/media/pci/intel/ipu3/Kconfig"
# with line:
source "drivers/media/pci/intel/Kconfig"
```

* Enable the following settings in .config

```
CONFIG_VIDEO_INTEL_IPU6=m
CONFIG_IPU_ISYS_BRIDGE=y
CONFIG_IPU_SINGLE_BE_SOC_DEVICE=n
CONFIG_INTEL_SKL_INT3472=m
```

### Build outside kernel source tree
* requires kernel header installed on building environment
* requires `patch.diff` patched on target kernel

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

