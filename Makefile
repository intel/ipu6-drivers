# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2022 Intel Corporation.

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

KV_IVSC := 6.6.0
KV_IPU_BRIDGE := 6.6.0
KV_OV2740 := 6.8.0
KV_OV05C10 := 6.8.0

KERNEL_SRC ?= /lib/modules/$(KERNELRELEASE)/build
MODSRC := $(shell pwd)

ifeq ($(call version_lt,$(KERNEL_VERSION),$(KV_IVSC)),true)
$(warning build ljca ivsc)
obj-m += ljca.o
ljca-y := drivers/mfd/ljca.o

obj-m += spi-ljca.o
spi-ljca-y := drivers/spi/spi-ljca.o

obj-m += gpio-ljca.o
gpio-ljca-y := drivers/gpio/gpio-ljca.o

obj-m += i2c-ljca.o
i2c-ljca-y := drivers/i2c/busses/i2c-ljca.o

obj-m += mei-vsc.o
mei-vsc-y := drivers/misc/mei/spi-vsc.o
mei-vsc-y += drivers/misc/mei/hw-vsc.o

obj-m += intel_vsc.o
intel_vsc-y := drivers/misc/ivsc/intel_vsc.o

obj-m += mei_csi.o
mei_csi-y := drivers/misc/ivsc/mei_csi.o

obj-m += mei_ace.o
mei_ace-y := drivers/misc/ivsc/mei_ace.o

obj-m += mei_pse.o
mei_pse-y := drivers/misc/ivsc/mei_pse.o

obj-m += mei_ace_debug.o
mei_ace_debug-y := drivers/misc/ivsc/mei_ace_debug.o

export CONFIG_INTEL_VSC = y

endif

export CONFIG_VIDEO_INTEL_IPU6 = m
export CONFIG_IPU_SINGLE_BE_SOC_DEVICE = n
export CONFIG_INTEL_SKL_INT3472 = m
# export CONFIG_POWER_CTRL_LOGIC = m
ifeq ($(call version_lt,$(KERNEL_VERSION),$(KV_IPU_BRIDGE)),true)
export CONFIG_IPU_ISYS_BRIDGE = y
export CONFIG_IPU_BRIDGE = n
endif
export EXTERNAL_BUILD = 1
obj-y += drivers/media/pci/intel/

export CONFIG_VIDEO_HM11B1 = m
export CONFIG_VIDEO_OV01A1S = m
export CONFIG_VIDEO_OV01A10 = m
export CONFIG_VIDEO_OV02C10 = m
export CONFIG_VIDEO_OV02E10 = m
export CONFIG_VIDEO_HM2170 = m
export CONFIG_VIDEO_HM2172 = m
export CONFIG_VIDEO_HI556 = m
export CONFIG_VIDEO_GC5035 = m

ifeq ($(call version_lt,$(KERNEL_VERSION),$(KV_OV2740)),true)
export CONFIG_VIDEO_OV2740 = m
endif

ifeq ($(call version_lt,$(KERNEL_VERSION),$(KV_OV05C10)),false)
export CONFIG_VIDEO_OV05C10 = m
endif

obj-y += drivers/media/i2c/

ifeq ($(call version_lt,$(KERNEL_VERSION),$(KV_IVSC)),true)
ccflags-y += -I$(src)/backport-include/drivers/misc/mei/
endif

subdir-ccflags-y += -I$(src)/include/ \
	-DCONFIG_VIDEO_V4L2_SUBDEV_API

subdir-ccflags-$(CONFIG_INTEL_VSC) += \
        -DCONFIG_INTEL_VSC
subdir-ccflags-$(CONFIG_IPU_ISYS_BRIDGE) += \
	-DCONFIG_IPU_ISYS_BRIDGE
subdir-ccflags-$(CONFIG_IPU_BRIDGE) += \
	-DCONFIG_IPU_BRIDGE
subdir-ccflags-$(CONFIG_IPU_SINGLE_BE_SOC_DEVICE) += \
	-DCONFIG_IPU_SINGLE_BE_SOC_DEVICE
subdir-ccflags-$(CONFIG_INTEL_SKL_INT3472) += \
	-DCONFIG_INTEL_SKL_INT3472
subdir-ccflags-$(CONFIG_POWER_CTRL_LOGIC) += \
	-DCONFIG_POWER_CTRL_LOGIC
subdir-ccflags-y += $(subdir-ccflags-m)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) modules
modules_install:
	$(MAKE) INSTALL_MOD_DIR=updates -C $(KERNEL_SRC) M=$(MODSRC) modules_install
clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) clean
