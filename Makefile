# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2025 Intel Corporation.

KERNELRELEASE ?= $(shell uname -r)
KERNEL_VERSION := $(shell echo $(KERNELRELEASE) | sed 's/[^0-9.]*\([0-9.]*\).*/\1/')

version_lt = $(shell \
    v1=$(1); \
    v2=$(2); \
    IFS='.'; \
    set -- $$v1; i=$$1; j=$$2; k=$$3; \
    set -- $$v2; a=$$1; b=$$2; c=$$3; \
    if [ "$$i" -lt "$$a" ]; then \
        echo "true"; \
    elif [ "$$i" -eq "$$a" ] && [ "$$j" -lt "$$b" ]; then \
        echo "true"; \
    elif [ "$$i" -eq "$$a" ] && [ "$$j" -eq "$$b" ] && [ "$$k" -lt "$$c" ]; then \
        echo "true"; \
    else \
        echo "false"; \
    fi)

KV_IPU6_ISYS := 6.10.0
KV_I2C_KV_I2C_MAX9X_ISX031 := 6.12.15

KERNEL_SRC ?= /lib/modules/$(KERNELRELEASE)/build
MODSRC := $(shell pwd)
export EXTERNAL_BUILD = 1

export CONFIG_VIDEO_INTEL_IPU6 = m
export CONFIG_VIDEO_INTEL_IPU6_ISYS_RESET=y
export CONFIG_INTEL_SKL_INT3472 = m
export CONFIG_INTEL_IPU6_ACPI = m

ifeq ($(call version_lt,$(KERNEL_VERSION),$(KV_IPU6_ISYS)),true)
obj-y += drivers/media/pci/intel/
else
obj-y += drivers/media/pci/intel/ipu6/
endif

export CONFIG_VIDEO_LT6911UXE = m
export CONFIG_VIDEO_LT6911UXC = m
export CONFIG_VIDEO_AR0234 = m

# kernel version >= 6.12.15
export CONFIG_VIDEO_ISX031=m
export CONFIG_VIDEO_MAX9X=m

obj-y += drivers/media/i2c/

ifeq ($(call version_lt,$(KERNEL_VERSION),$(KV_I2C_KV_I2C_MAX9X_ISX031)),false)
obj-y += drivers/media/platform/intel/
endif

subdir-ccflags-y += -I$(src)/include/ \
	-DCONFIG_VIDEO_V4L2_SUBDEV_API

subdir-ccflags-$(CONFIG_IPU_ISYS_BRIDGE) += \
	-DCONFIG_IPU_ISYS_BRIDGE
subdir-ccflags-$(CONFIG_IPU_BRIDGE) += \
	-DCONFIG_IPU_BRIDGE

subdir-ccflags-$(CONFIG_INTEL_SKL_INT3472) += \
	-DCONFIG_INTEL_SKL_INT3472

subdir-ccflags-y += $(subdir-ccflags-m)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) modules
modules_install:
	$(MAKE) INSTALL_MOD_DIR=updates -C $(KERNEL_SRC) M=$(MODSRC) modules_install
clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) clean

