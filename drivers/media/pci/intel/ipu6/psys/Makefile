# SPDX-License-Identifier: GPL-2.0-only

is_kernel_lt_6_10 = $(shell if [ $$(printf "6.10\n$(KERNELVERSION)" | sort -V | head -n1) != "6.10" ]; then echo 1; fi)
ifeq ($(is_kernel_lt_6_10), 1)
ifneq ($(EXTERNAL_BUILD), 1)
src := $(srctree)/$(src)
endif
endif

intel-ipu6-psys-objs			+= ipu-psys.o \
					   ipu-fw-psys.o \
					   ipu-fw-resources.o \
					   ipu-resources.o \
					   ipu6-psys.o \
					   ipu6-ppg.o \
					   ipu6-l-scheduler.o \
					   ipu6-fw-resources.o \
					   ipu6se-fw-resources.o \
					   ipu6ep-fw-resources.o

obj-$(CONFIG_VIDEO_INTEL_IPU6)		+= intel-ipu6-psys.o

ifeq ($(is_kernel_lt_6_10), 1)
ccflags-y += -I$(src)/../ipu6/
endif
# Since 6.10 the private IPU6 headers (ipu6.h, ipu6-bus.h, ipu6-dma.h, ...) belong to the kernel.
# Use the ones shipped with the kernel (headers package or source tree) when they exist: a
# bundled copy goes stale as the in-tree struct layouts change.
ipu6_kernel_hdrs := $(KERNEL_SRC)/drivers/media/pci/intel/ipu6
ifneq ($(wildcard $(ipu6_kernel_hdrs)/ipu6.h),)
ccflags-y += -I$(ipu6_kernel_hdrs)
endif

ccflags-y += -I$(src)/../
ccflags-y += -I$(src)/../../
