# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2022 Intel Corporation.

export CONFIG_VIDEO_INTEL_IPU6 = m
export CONFIG_IPU_ISYS_BRIDGE = y
obj-y += drivers/media/pci/intel/

# export CONFIG_POWER_CTRL_LOGIC = m
export CONFIG_INTEL_SKL_INT3472 = m
export CONFIG_VIDEO_HM11B1 = m
export CONFIG_VIDEO_OV01A1S = m
export CONFIG_VIDEO_OV01A10 = m
export CONFIG_VIDEO_OV02C10 = m
export CONFIG_VIDEO_OV2740 = m
obj-y += drivers/media/i2c/

KERNELRELEASE ?= $(shell uname -r)
KERNEL_SRC ?= /lib/modules/$(KERNELRELEASE)/build
MODSRC := $(shell pwd)
subdir-ccflags-y += -I$(src)/include/

subdir-ccflags-$(CONFIG_IPU_ISYS_BRIDGE) += \
	-DCONFIG_IPU_ISYS_BRIDGE
subdir-ccflags-$(CONFIG_INTEL_SKL_INT3472) += \
	-DCONFIG_INTEL_SKL_INT3472
subdir-ccflags-$(CONFIG_POWER_CTRL_LOGIC) += \
	-DCONFIG_POWER_CTRL_LOGIC
subdir-ccflags-y += $(subdir-ccflags-m)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) modules
modules_install:
	$(MAKE) INSTALL_MOD_DIR=extra -C $(KERNEL_SRC) M=$(MODSRC) modules_install
clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) clean
