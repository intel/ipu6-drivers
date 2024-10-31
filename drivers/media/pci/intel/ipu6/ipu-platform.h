/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2013 - 2024 Intel Corporation */

#ifndef IPU_PLATFORM_H
#define IPU_PLATFORM_H

#define IPU_NAME			"intel-ipu6"

#define IPU6SE_FIRMWARE_NAME		"intel/ipu6se_fw.bin"
#define IPU6EP_FIRMWARE_NAME		"intel/ipu6ep_fw.bin"
#define IPU6EPES_FIRMWARE_NAME		"intel/ipu6epes_fw.bin"
#define IPU6_FIRMWARE_NAME		"intel/ipu6_fw.bin"
#define IPU6EPMTL_FIRMWARE_NAME		"intel/ipu6epmtl_fw.bin"
#define IPU6EPADLN_FIRMWARE_NAME	"intel/ipu6epadln_fw.bin"
#define IPU6EPMTLES_FIRMWARE_NAME	"intel/ipu6epmtles_fw.bin"

#define IPU6SE_FIRMWARE_NAME_NEW	"intel/ipu/ipu6se_fw.bin"
#define IPU6EP_FIRMWARE_NAME_NEW	"intel/ipu/ipu6ep_fw.bin"
#define IPU6EPES_FIRMWARE_NAME_NEW	"intel/ipu/ipu6epes_fw.bin"
#define IPU6_FIRMWARE_NAME_NEW		"intel/ipu/ipu6_fw.bin"
#define IPU6EPMTL_FIRMWARE_NAME_NEW	"intel/ipu/ipu6epmtl_fw.bin"
#define IPU6EPADLN_FIRMWARE_NAME_NEW	"intel/ipu/ipu6epadln_fw.bin"
#define IPU6EPMTLES_FIRMWARE_NAME_NEW	"intel/ipu/ipu6epmtles_fw.bin"

#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA) \
	&& IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING)
/* array of struct ipu_spdata_rep terminated by NULL */
#define IPU_SPDATA_NAME		"ipu6v1_spdata.bin"
#endif

/*
 * The following definitions are encoded to the media_device's model field so
 * that the software components which uses IPU driver can get the hw stepping
 * information.
 */
#define IPU_MEDIA_DEV_MODEL_NAME		"ipu6-downstream"

#define IPU6SE_ISYS_NUM_STREAMS          IPU6SE_NONSECURE_STREAM_ID_MAX
#define IPU6_ISYS_NUM_STREAMS            IPU6_NONSECURE_STREAM_ID_MAX

/* declearations, definitions in ipu6.c */
extern struct ipu_isys_internal_pdata isys_ipdata;
extern struct ipu_psys_internal_pdata psys_ipdata;
extern const struct ipu_buttress_ctrl isys_buttress_ctrl;
extern const struct ipu_buttress_ctrl psys_buttress_ctrl;

/* definitions in ipu6-isys.c */
extern struct ipu_trace_block isys_trace_blocks[];
/* definitions in ipu6-psys.c */
extern struct ipu_trace_block psys_trace_blocks[];

#endif
