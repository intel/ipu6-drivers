# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2017 - 2024 Intel Corporation.

ifneq ($(EXTERNAL_BUILD), 1)
srcpath := $(srctree)
endif

ccflags-y += -DIPU_HAS_S2M -DIPU_TPG_FRAME_SYNC \
		-DIPU_ISYS_GPC -DI2C_DYNAMIC
ccflags-y += -DIPU_ISYS_RESET

intel-ipu6-objs				+= ../ipu.o \
					   ../ipu-bus.o \
					   ../ipu-dma.o \
					   ../ipu-mmu.o \
					   ../ipu-buttress.o \
					   ../ipu-trace.o \
					   ../ipu-cpd.o \
					   ipu6.o \
					   ../ipu-fw-com.o

obj-$(CONFIG_VIDEO_INTEL_IPU6)		+= intel-ipu6.o

intel-ipu6-isys-objs			+= ../ipu-isys.o \
					   ../ipu-isys-csi2.o \
					   ipu6-isys.o \
					   ipu6-isys-phy.o \
					   ipu6-isys-dwc-phy.o \
					   ipu6-isys-csi2.o \
					   ipu6-isys-gpc.o \
					   ../ipu-isys-csi2-be-soc.o \
					   ../ipu-fw-isys.o \
					   ../ipu-isys-video.o \
					   ../ipu-isys-queue.o \
					   ../ipu-isys-subdev.o
intel-ipu6-isys-objs			+= ../ipu-isys-csi2-be.o

obj-$(CONFIG_VIDEO_INTEL_IPU6)		+= intel-ipu6-isys.o

obj-$(CONFIG_VIDEO_INTEL_IPU6)		+= psys/

ccflags-y += -I$(srcpath)/$(src)/../../../../../include/
ccflags-y += -I$(srcpath)/$(src)/psys/
ccflags-y += -I$(srcpath)/$(src)/../
ccflags-y += -I$(srcpath)/$(src)/
