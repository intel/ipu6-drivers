# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2022 Intel Corporation.

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

export CONFIG_VIDEO_INTEL_IPU6 = m
export CONFIG_IPU_ISYS_BRIDGE = y
export CONFIG_INTEL_SKL_INT3472 = m
obj-y += drivers/media/pci/intel/

export CONFIG_VIDEO_HM11B1 = m
export CONFIG_VIDEO_OV01A1S = m
export CONFIG_VIDEO_OV01A10 = m
export CONFIG_VIDEO_OV02C10 = m
export CONFIG_VIDEO_OV2740 = m
export CONFIG_VIDEO_HM2170 = m
export CONFIG_VIDEO_HI556 = m
# export CONFIG_POWER_CTRL_LOGIC = m
obj-y += drivers/media/i2c/

KERNELRELEASE ?= $(shell uname -r)
KERNEL_SRC ?= /lib/modules/$(KERNELRELEASE)/build
MODSRC := $(shell pwd)

ccflags-y += -I$(src)/backport-include/drivers/misc/mei/

subdir-ccflags-y += -I$(src)/include/

subdir-ccflags-$(CONFIG_INTEL_VSC) += \
        -DCONFIG_INTEL_VSC
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
	$(MAKE) INSTALL_MOD_DIR=updates -C $(KERNEL_SRC) M=$(MODSRC) modules_install
clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODSRC) clean
