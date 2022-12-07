# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2021 Intel Corporation.

export EXTERNAL_BUILD = 1

export CONFIG_VIDEO_INTEL_IPU6 = m
export CONFIG_VIDEO_INTEL_IPU_SOC = y
export CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA = y
export CONFIG_INTEL_SKL_INT3472 = m
export CONFIG_INTEL_IPU6_ACPI = m
obj-y += drivers/media/pci/intel/

export CONFIG_VIDEO_AR0234 = m
export CONFIG_VIDEO_LT6911UXC = m
export CONFIG_VIDEO_D4XX = m
obj-y += drivers/media/i2c/

KERNEL_SRC := /lib/modules/$(shell uname -r)/build
MODSRC := $(shell pwd)
ccflags-y += -I$(MODSRC)/include/

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) modules
modules_install:
	$(MAKE) INSTALL_MOD_DIR=/extra -C $(KERNEL_SRC) M=$(MODSRC) modules_install
clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) clean
