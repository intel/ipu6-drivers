# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2010 - 2025 Intel Corporation.

is_kernel_lt_6_15 = $(shell if [ $$(printf "6.15\n$(KERNELVERSION)" | sort -V | head -n1) != "6.15" ]; then echo 1; else echo 0; fi)
is_kernel_lt_6_12_15 = $(shell if [ $$(printf "6.12.15\n$(KERNELVERSION)" | sort -V | head -n1) != "6.12.15" ]; then echo 1; else echo 0; fi)

obj-$(CONFIG_VIDEO_AR0234) += ar0234.o
obj-$(CONFIG_VIDEO_LT6911UXC) += lt6911uxc.o
ifeq ($(is_kernel_lt_6_15), 1)
obj-$(CONFIG_VIDEO_LT6911UXE) += lt6911uxe.o
endif

ifeq ($(is_kernel_lt_6_12_15), 0)
obj-$(CONFIG_VIDEO_ISX031) += isx031.o
obj-$(CONFIG_VIDEO_MAX9X) += max9x/
endif
