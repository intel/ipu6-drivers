# SPDX-License-Identifier: GPL-2.0-only

is_kernel_lt_6_10 = $(shell if [ $$(printf "6.10\n$(KERNELVERSION)" | sort -V | head -n1) != "6.10" ]; then echo 1; fi)
ifeq ($(is_kernel_lt_6_10), 1)
ifneq ($(EXTERNAL_BUILD), 1)
src := $(srctree)/$(src)
endif
endif

ccflags-y += -DIPU_OTF_SUPPORT
ccflags-y += -DIPU_PSYS_GPC

intel-ipu6-psys-objs			+= ipu-psys.o \
					   ipu6-psys.o \
					   ipu-resources.o \
					   ipu6-psys-gpc.o \
					   ipu6-l-scheduler.o \
					   ipu6-ppg.o

intel-ipu6-psys-objs			+= ipu-fw-resources.o \
					   ipu6-fw-resources.o \
					   ipu6se-fw-resources.o \
					   ipu6ep-fw-resources.o \
					   ipu-fw-psys.o

ifeq ($(CONFIG_COMPAT),y)
intel-ipu6-psys-objs			+= ipu-psys-compat32.o
endif

obj-$(CONFIG_VIDEO_INTEL_IPU6)		+= intel-ipu6-psys.o

ifeq ($(is_kernel_lt_6_10), 1)
ccflags-y += -I$(src)/../ipu6/
endif
ccflags-y += -I$(src)/../
