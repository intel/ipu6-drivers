# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2021 Intel Corporation.

export CONFIG_VIDEO_INTEL_IPU6 = m
obj-y += drivers/media/pci/intel/

export CONFIG_VIDEO_HM11B1 = m
export CONFIG_VIDEO_OV01A1S = m
export CONFIG_VIDEO_OV01A10 = m
export CONFIG_VIDEO_OV02C10 = m
export CONFIG_POWER_CTRL_LOGIC = m
obj-y += drivers/media/i2c/

KERNELRELEASE	?= `uname -r`
KERNEL_SRC := /lib/modules/$(KERNELRELEASE)/build
MODSRC := $(shell pwd)
subdir-ccflags-y += -I$(src)/include/

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) modules
modules_install:
	$(MAKE) INSTALL_MOD_DIR=/extra -C $(KERNEL_SRC) M=$(MODSRC) modules_install
clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) clean
