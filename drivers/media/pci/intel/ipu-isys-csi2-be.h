/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2014 - 2024 Intel Corporation */

#ifndef IPU_ISYS_CSI2_BE_H
#define IPU_ISYS_CSI2_BE_H

#include <media/media-entity.h>
#include <media/v4l2-device.h>

#include "ipu-isys-queue.h"
#include "ipu-isys-subdev.h"
#include "ipu-isys-video.h"
#include "ipu-platform-isys.h"

struct ipu_isys_csi2_be_pdata;
struct ipu_isys;

#define CSI2_BE_PAD_SINK		0
#define CSI2_BE_PAD_SOURCE		1

#define NR_OF_CSI2_BE_PADS		2
#define NR_OF_CSI2_BE_SOURCE_PADS	1
#define NR_OF_CSI2_BE_SINK_PADS		1

#define INVALIA_VC_ID -1
#define NR_OF_CSI2_BE_SOC_SOURCE_PADS	NR_OF_CSI2_BE_SOC_STREAMS
#define NR_OF_CSI2_BE_SOC_SINK_PADS	1
#define CSI2_BE_SOC_PAD_SINK 0
#define CSI2_BE_SOC_PAD_SOURCE(n)	\
	({ typeof(n) __n = (n);  \
	   (__n) >= NR_OF_CSI2_BE_SOC_SOURCE_PADS ? \
		(NR_OF_CSI2_BE_SOC_SOURCE_PADS - 1) : \
		((__n) + NR_OF_CSI2_BE_SOC_SINK_PADS); })
#define NR_OF_CSI2_BE_SOC_PADS \
	(NR_OF_CSI2_BE_SOC_SOURCE_PADS + NR_OF_CSI2_BE_SOC_SINK_PADS)

#define CSI2_BE_CROP_HOR	BIT(0)
#define CSI2_BE_CROP_VER	BIT(1)
#define CSI2_BE_CROP_MASK	(CSI2_BE_CROP_VER | CSI2_BE_CROP_HOR)

/*
 * struct ipu_isys_csi2_be
 */
struct ipu_isys_csi2_be {
	struct ipu_isys_csi2_be_pdata *pdata;
	struct ipu_isys_subdev asd;
	struct ipu_isys_video av;
};

struct ipu_isys_csi2_be_soc {
	struct ipu_isys_csi2_be_pdata *pdata;
	struct ipu_isys_subdev asd;
	struct ipu_isys_video av[NR_OF_CSI2_BE_SOC_SOURCE_PADS];
};

#define to_ipu_isys_csi2_be(sd)	\
	container_of(to_ipu_isys_subdev(sd), \
	struct ipu_isys_csi2_be, asd)

#define to_ipu_isys_csi2_be_soc(sd)	\
	container_of(to_ipu_isys_subdev(sd), \
	struct ipu_isys_csi2_be_soc, asd)

int ipu_isys_csi2_be_init(struct ipu_isys_csi2_be *csi2_be,
			  struct ipu_isys *isys);
void ipu_isys_csi2_be_cleanup(struct ipu_isys_csi2_be *csi2_be);
int ipu_isys_csi2_be_soc_init(struct ipu_isys_csi2_be_soc *csi2_be_soc,
			      struct ipu_isys *isys, int index);
void ipu_isys_csi2_be_soc_cleanup(struct ipu_isys_csi2_be_soc *csi2_be);

#endif /* IPU_ISYS_CSI2_BE_H */
