# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2010 - 2020 Intel Corporation.

export EXTERNAL_BUILD=1
export CONFIG_VIDEO_INTEL_IPU6=m
export CONFIG_VIDEO_AR0234=m
export CONFIG_VIDEO_LT6911UXC=m
export CONFIG_INTEL_IPU6_ACPI=y
export CONFIG_INTEL_IPU6_TGLRVP_PDATA=y
export CONFIG_INTEL_IPU6_ADLRVP_PDATA=y

MODSRC := $(shell pwd)
ccflags-y += -I$(MODSRC)/include/

KERNEL_SRC=/usr/src/vied-viedlin-intel-next


obj-y				+= drivers/media/pci/intel/
obj-y				+= drivers/media/platform/intel/
obj-y				+= drivers/media/i2c/

ifeq ($(EXTERNAL_BUILD), 1)
all:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) clean

endif
