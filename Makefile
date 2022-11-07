# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2022 Intel Corporation.

# for build in makefile
export CONFIG_VIDEO_INTEL_IPU6 = m
export CONFIG_INTEL_SKL_INT3472 = m
obj-y += drivers/media/pci/intel/

export CONFIG_VIDEO_AR0234 = m
export CONFIG_VIDEO_LT6911UXC = m
export CONFIG_VIDEO_D4XX = m
obj-y += drivers/media/i2c/

export CONFIG_INTEL_IPU6_ACPI = m
export CONFIG_INTEL_IPU6_ADLRVP_PDATA = y
export CONFIG_INTEL_IPU6_TGLRVP_PDATA = y
export CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA = y
export CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING = y
obj-y += drivers/media/platform/intel/

KERNELRELEASE ?= $(shell uname -r)
KERNEL_SRC ?= /lib/modules/$(KERNELRELEASE)/build
MODSRC := $(shell pwd)

subdir-ccflags-y += -I$(src)/include/

# for add into kernel config
subdir-ccflags-$(CONFIG_INTEL_SKL_INT3472) += \
	-DCONFIG_INTEL_SKL_INT3472
subdir-ccflags-$(CONFIG_VIDEO_AR0234) += \
	-DCONFIG_VIDEO_AR0234
subdir-ccflags-$(CONFIG_VIDEO_LT6911UXC) += \
	-DCONFIG_VIDEO_LT6911UXC
subdir-ccflags-$(CONFIG_VIDEO_D4XX) += \
	-DCONFIG_VIDEO_D4XX
subdir-ccflags-$(CONFIG_INTEL_IPU6_ACPI) += \
	-DCONFIG_INTEL_IPU6_ACPI
subdir-ccflags-$(CONFIG_INTEL_IPU6_ADLRVP_PDATA) += \
	-DCONFIG_INTEL_IPU6_ADLRVP_PDATA
subdir-ccflags-$(CONFIG_INTEL_IPU6_TGLRVP_PDATA) += \
	-DCONFIG_INTEL_IPU6_TGLRVP_PDATA
subdir-ccflags-$(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA) += \
	-DCONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA
subdir-ccflags-$(CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING) += \
	-DCONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING

subdir-ccflags-y += $(subdir-ccflags-m)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) modules
modules_install:
	$(MAKE) INSTALL_MOD_DIR=updates -C $(KERNEL_SRC) M=$(MODSRC) modules_install
clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) clean
