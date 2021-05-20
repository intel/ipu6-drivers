# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2021 Intel Corporation.

export EXTERNAL_BUILD = 1

export CONFIG_VIDEO_INTEL_IPU6 = m
obj-y += drivers/media/pci/intel/

export CONFIG_VIDEO_HM11B1 = m
export CONFIG_VIDEO_OV01A1S = m
export CONFIG_POWER_CTRL_LOGIC = m
obj-y += drivers/media/i2c/

export CONFIG_INTEL_LPSS_USB = m
obj-y += drivers/usb/intel_ulpss/

KERNEL_SRC := /lib/modules/$(shell uname -r)/build
MODSRC := $(shell pwd)
ccflags-y += -I$(MODSRC)/include/

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) modules
modules_install:
	$(MAKE) INSTALL_MOD_DIR=/extra -C $(KERNEL_SRC) M=$(MODSRC) modules_install
clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) clean
