/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/* Copyright (C) 2016 - 2020 Intel Corporation */

#ifndef UAPI_LINUX_IPU_ISYS_H
#define UAPI_LINUX_IPU_ISYS_H

#define V4L2_CID_IPU_BASE	(V4L2_CID_USER_BASE + 0x1080)

#define V4L2_CID_IPU_STORE_CSI2_HEADER	(V4L2_CID_IPU_BASE + 2)

#define VIDIOC_IPU_GET_DRIVER_VERSION \
	_IOWR('v', BASE_VIDIOC_PRIVATE + 3, uint32_t)

#endif /* UAPI_LINUX_IPU_ISYS_H */
