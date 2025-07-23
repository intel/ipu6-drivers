# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2010 - 2025 Intel Corporation.

ifneq ($(EXTERNAL_BUILD), 1)
srcpath := $(srctree)
endif

ccflags-y += -I$(srcpath)/$(src)/../../../../include/
ccflags-y += -I$(srcpath)/$(src)/../../pci/intel/ipu6/

obj-$(CONFIG_INTEL_IPU6_ACPI)         += ipu6-acpi.o \
					ipu6-acpi-pdata.o \
					ipu6-acpi-common.o
