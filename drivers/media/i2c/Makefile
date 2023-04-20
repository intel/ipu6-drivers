# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2021 Intel Corporation.

ifneq ($(EXTERNAL_BUILD), 1)
srcpath := $(srctree)
endif

ccflags-y += -I$(srcpath)/$(src)/../../../include/
ccflags-y += -DCONFIG_VIDEO_D4XX_SERDES

obj-$(CONFIG_VIDEO_AR0234) += ar0234.o
obj-$(CONFIG_VIDEO_LT6911UXC) += lt6911uxc.o
obj-$(CONFIG_I2C_IOEXPANDER_SER_MAX9295) += max9295.o
obj-$(CONFIG_I2C_IOEXPANDER_DESER_MAX9296) += max9296.o
obj-$(CONFIG_VIDEO_D4XX) += d4xx.o
