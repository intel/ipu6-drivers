// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2014 - 2024 Intel Corporation

#include <linux/device.h>
#include <linux/module.h>

#include <media/ipu-isys.h>
#include <media/media-entity.h>
#include <media/v4l2-device.h>

#include "ipu.h"
#include "ipu-bus.h"
#include "ipu-isys.h"
#include "ipu-isys-csi2-be.h"
#include "ipu-isys-subdev.h"
#include "ipu-isys-video.h"

extern int vnode_num;

/*
 * Raw bayer format pixel order MUST BE MAINTAINED in groups of four codes.
 * Otherwise pixel order calculation below WILL BREAK!
 */
static const u32 csi2_be_soc_supported_codes_pad[] = {
	MEDIA_BUS_FMT_Y10_1X10,
	MEDIA_BUS_FMT_RGB565_1X16,
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_UYVY8_1X16,
	MEDIA_BUS_FMT_YUYV8_1X16,
	MEDIA_BUS_FMT_VYUY8_1X16,
	MEDIA_BUS_FMT_SBGGR12_1X12,
	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SGRBG12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,
	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SBGGR8_1X8,
	MEDIA_BUS_FMT_SGBRG8_1X8,
	MEDIA_BUS_FMT_SGRBG8_1X8,
	MEDIA_BUS_FMT_SRGGB8_1X8,
	MEDIA_BUS_FMT_Y8_1X8,
	MEDIA_BUS_FMT_FIXED,
	0,
};

/*
 * Raw bayer format pixel order MUST BE MAINTAINED in groups of four codes.
 * Otherwise pixel order calculation below WILL BREAK!
 */
static const u32 csi2_be_soc_supported_raw_bayer_codes_pad[] = {
	MEDIA_BUS_FMT_SBGGR12_1X12,
	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SGRBG12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,
	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SBGGR8_1X8,
	MEDIA_BUS_FMT_SGBRG8_1X8,
	MEDIA_BUS_FMT_SGRBG8_1X8,
	MEDIA_BUS_FMT_SRGGB8_1X8,
	MEDIA_BUS_FMT_Y8_1X8,
	0,
};

static int get_supported_code_index(u32 code)
{
	int i;

	for (i = 0; csi2_be_soc_supported_raw_bayer_codes_pad[i]; i++) {
		if (csi2_be_soc_supported_raw_bayer_codes_pad[i] == code)
			return i;
	}
	return -EINVAL;
}

static const u32 *csi2_be_soc_supported_codes[NR_OF_CSI2_BE_SOC_PADS];

static struct v4l2_subdev_internal_ops csi2_be_soc_sd_internal_ops = {
	.open = ipu_isys_subdev_open,
	.close = ipu_isys_subdev_close,
};

static const struct v4l2_subdev_core_ops csi2_be_soc_sd_core_ops = {
};

static const struct v4l2_ctrl_config compression_ctrl_cfg = {
	.ops = NULL,
	.id = V4L2_CID_IPU_ISYS_COMPRESSION,
	.name = "ISYS BE-SOC compression",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 0,
};

static int set_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static const struct v4l2_subdev_video_ops csi2_be_soc_sd_video_ops = {
	.s_stream = set_stream,
};

static int
__subdev_link_validate(struct v4l2_subdev *sd, struct media_link *link,
		       struct v4l2_subdev_format *source_fmt,
		       struct v4l2_subdev_format *sink_fmt)
{
	struct media_pipeline *mp = media_entity_pipeline(&sd->entity);
	struct ipu_isys_pipeline *ip = container_of(mp,
						    struct ipu_isys_pipeline,
						    pipe);

	ip->csi2_be_soc = to_ipu_isys_csi2_be_soc(sd);
	return ipu_isys_subdev_link_validate(sd, link, source_fmt, sink_fmt);
}

static int
ipu_isys_csi2_be_soc_set_sel(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			     struct v4l2_subdev_pad_config *cfg,
#else
			     struct v4l2_subdev_state *state,
#endif
			     struct v4l2_subdev_selection *sel)
{
	struct ipu_isys_subdev *asd = to_ipu_isys_subdev(sd);
	struct media_pad *pad = &asd->sd.entity.pads[sel->pad];

	if (sel->target == V4L2_SEL_TGT_CROP &&
	    pad->flags & MEDIA_PAD_FL_SOURCE &&
	    asd->valid_tgts[sel->pad].crop) {
		enum isys_subdev_prop_tgt tgt =
		    IPU_ISYS_SUBDEV_PROP_TGT_SOURCE_CROP;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		struct v4l2_mbus_framefmt *ffmt =
			__ipu_isys_get_ffmt(sd, cfg, sel->pad, sel->which);
#else
		struct v4l2_mbus_framefmt *ffmt =
			__ipu_isys_get_ffmt(sd, state, sel->pad, sel->which);
#endif

		if (get_supported_code_index(ffmt->code) < 0) {
			/* Non-bayer formats can't be odd lines cropped */
			sel->r.left &= ~1;
			sel->r.top &= ~1;
		}

		sel->r.width = clamp(sel->r.width, IPU_ISYS_MIN_WIDTH,
				     IPU_ISYS_MAX_WIDTH);

		sel->r.height = clamp(sel->r.height, IPU_ISYS_MIN_HEIGHT,
				      IPU_ISYS_MAX_HEIGHT);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		*__ipu_isys_get_selection(sd, cfg, sel->target, sel->pad,
					  sel->which) = sel->r;
		ipu_isys_subdev_fmt_propagate(sd, cfg, NULL, &sel->r,
					      tgt, sel->pad, sel->which);
#else
		*__ipu_isys_get_selection(sd, state, sel->target, sel->pad,
					  sel->which) = sel->r;
		ipu_isys_subdev_fmt_propagate(sd, state, NULL, &sel->r,
					      tgt, sel->pad, sel->which);
#endif
		return 0;
	}
	return -EINVAL;
}

static const struct v4l2_subdev_pad_ops csi2_be_soc_sd_pad_ops = {
	.link_validate = __subdev_link_validate,
	.get_fmt = ipu_isys_subdev_get_ffmt,
	.set_fmt = ipu_isys_subdev_set_ffmt,
	.get_selection = ipu_isys_subdev_get_sel,
	.set_selection = ipu_isys_csi2_be_soc_set_sel,
	.enum_mbus_code = ipu_isys_subdev_enum_mbus_code,
};

static struct v4l2_subdev_ops csi2_be_soc_sd_ops = {
	.core = &csi2_be_soc_sd_core_ops,
	.video = &csi2_be_soc_sd_video_ops,
	.pad = &csi2_be_soc_sd_pad_ops,
};

static struct media_entity_operations csi2_be_soc_entity_ops = {
};

static void csi2_be_soc_set_ffmt(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
				 struct v4l2_subdev_fh *cfg,
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
				 struct v4l2_subdev_pad_config *cfg,
#else
				 struct v4l2_subdev_state *state,
#endif
				 struct v4l2_subdev_format *fmt)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
	struct v4l2_mbus_framefmt *ffmt =
		__ipu_isys_get_ffmt(sd, cfg, fmt->pad,
				    fmt->which);
#else
	struct v4l2_mbus_framefmt *ffmt =
		__ipu_isys_get_ffmt(sd, state, fmt->pad,
				    fmt->which);
#endif

	if (sd->entity.pads[fmt->pad].flags & MEDIA_PAD_FL_SINK) {
		if (fmt->format.field != V4L2_FIELD_ALTERNATE)
			fmt->format.field = V4L2_FIELD_NONE;
		*ffmt = fmt->format;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		ipu_isys_subdev_fmt_propagate(sd, cfg, &fmt->format,
					      NULL,
					      IPU_ISYS_SUBDEV_PROP_TGT_SINK_FMT,
					      fmt->pad, fmt->which);
#else
		ipu_isys_subdev_fmt_propagate(sd, state, &fmt->format,
					      NULL,
					      IPU_ISYS_SUBDEV_PROP_TGT_SINK_FMT,
					      fmt->pad, fmt->which);
#endif
	} else if (sd->entity.pads[fmt->pad].flags & MEDIA_PAD_FL_SOURCE) {
		struct v4l2_mbus_framefmt *sink_ffmt;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		struct v4l2_rect *r = __ipu_isys_get_selection(sd, cfg,
			V4L2_SEL_TGT_CROP, fmt->pad, fmt->which);
#else
		struct v4l2_rect *r = __ipu_isys_get_selection(sd, state,
			V4L2_SEL_TGT_CROP, fmt->pad, fmt->which);
#endif
		struct ipu_isys_subdev *asd = to_ipu_isys_subdev(sd);
		u32 code;
		int idx;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		sink_ffmt = __ipu_isys_get_ffmt(sd, cfg, 0, fmt->which);
#else
		sink_ffmt = __ipu_isys_get_ffmt(sd, state, 0, fmt->which);
#endif
		code = sink_ffmt->code;
		idx = get_supported_code_index(code);

		if (asd->valid_tgts[fmt->pad].crop && idx >= 0) {
			int crop_info = 0;

			/* Only croping odd line at top side. */
			if (r->top & 1)
				crop_info |= CSI2_BE_CROP_VER;

			code = csi2_be_soc_supported_raw_bayer_codes_pad
				[((idx & CSI2_BE_CROP_MASK) ^ crop_info)
				+ (idx & ~CSI2_BE_CROP_MASK)];

		}
		ffmt->code = fmt->format.code;
		ffmt->width = r->width;
		ffmt->height = r->height;
		ffmt->field = sink_ffmt->field;

	}
}

void ipu_isys_csi2_be_soc_cleanup(struct ipu_isys_csi2_be_soc *csi2_be_soc)
{
	int i;

	v4l2_device_unregister_subdev(&csi2_be_soc->asd.sd);
	ipu_isys_subdev_cleanup(&csi2_be_soc->asd);
	for (i = 0; i < vnode_num; i++) {
		v4l2_ctrl_handler_free(&csi2_be_soc->av[i].ctrl_handler);
		ipu_isys_video_cleanup(&csi2_be_soc->av[i]);
	}
}

int ipu_isys_csi2_be_soc_init(struct ipu_isys_csi2_be_soc *csi2_be_soc,
			      struct ipu_isys *isys, int index)
{
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		.pad = CSI2_BE_SOC_PAD_SINK,
		.format = {
			   .width = 4096,
			   .height = 3072,
			   },
	};
	int rval, i;

	csi2_be_soc->asd.sd.entity.ops = &csi2_be_soc_entity_ops;
	csi2_be_soc->asd.isys = isys;

	rval = ipu_isys_subdev_init(&csi2_be_soc->asd,
				    &csi2_be_soc_sd_ops, 0,
				    NR_OF_CSI2_BE_SOC_PADS,
				    NR_OF_CSI2_BE_SOC_SOURCE_PADS,
				    NR_OF_CSI2_BE_SOC_SINK_PADS, 0,
				    CSI2_BE_SOC_PAD_SOURCE(0),
				    CSI2_BE_SOC_PAD_SINK);
	if (rval)
		goto fail;

	for (i = CSI2_BE_SOC_PAD_SOURCE(0);
	     i < NR_OF_CSI2_BE_SOC_SOURCE_PADS + CSI2_BE_SOC_PAD_SOURCE(0);
	     i++) {
		csi2_be_soc->asd.valid_tgts[i].crop = true;
	}

	for (i = 0; i < NR_OF_CSI2_BE_SOC_PADS; i++)
		csi2_be_soc_supported_codes[i] =
			csi2_be_soc_supported_codes_pad;
	csi2_be_soc->asd.supported_codes = csi2_be_soc_supported_codes;
	csi2_be_soc->asd.be_mode = IPU_BE_SOC;
	csi2_be_soc->asd.isl_mode = IPU_ISL_OFF;
	csi2_be_soc->asd.set_ffmt = csi2_be_soc_set_ffmt;

	fmt.pad = CSI2_BE_SOC_PAD_SINK;
	ipu_isys_subdev_set_ffmt(&csi2_be_soc->asd.sd, NULL, &fmt);
	csi2_be_soc->asd.sd.internal_ops = &csi2_be_soc_sd_internal_ops;

	snprintf(csi2_be_soc->asd.sd.name, sizeof(csi2_be_soc->asd.sd.name),
		 IPU_ISYS_ENTITY_PREFIX " CSI2 BE SOC %d", index);
	v4l2_set_subdevdata(&csi2_be_soc->asd.sd, &csi2_be_soc->asd);

	rval = v4l2_device_register_subdev(&isys->v4l2_dev,
					   &csi2_be_soc->asd.sd);
	if (rval) {
		dev_info(&isys->adev->dev, "can't register v4l2 subdev\n");
		goto fail;
	}

	for (i = 0; i < vnode_num; i++) {
		if (!index)
			snprintf(csi2_be_soc->av[i].vdev.name,
				 sizeof(csi2_be_soc->av[i].vdev.name),
				 IPU_ISYS_ENTITY_PREFIX " BE SOC capture %d", i);
		else
			snprintf(csi2_be_soc->av[i].vdev.name,
				 sizeof(csi2_be_soc->av[i].vdev.name),
				 IPU_ISYS_ENTITY_PREFIX " BE SOC %d capture %d", index, i);
		/*
		 * Pin type could be overwritten for YUV422 to I420 case, at
		 * set_format phase
		 */
		csi2_be_soc->av[i].aq.css_pin_type =
		    IPU_FW_ISYS_PIN_TYPE_RAW_SOC;
		csi2_be_soc->av[i].isys = isys;
		csi2_be_soc->av[i].pfmts = ipu_isys_pfmts_be_soc;

		csi2_be_soc->av[i].try_fmt_vid_mplane =
		    ipu_isys_video_try_fmt_vid_mplane_default;
		csi2_be_soc->av[i].prepare_fw_stream =
		    ipu_isys_prepare_fw_cfg_default;
		csi2_be_soc->av[i].aq.buf_prepare = ipu_isys_buf_prepare;
		csi2_be_soc->av[i].aq.fill_frame_buff_set_pin =
		    ipu_isys_buffer_to_fw_frame_buff_pin;
		csi2_be_soc->av[i].aq.link_fmt_validate =
		    ipu_isys_link_fmt_validate;
		csi2_be_soc->av[i].aq.vbq.buf_struct_size =
		    sizeof(struct ipu_isys_video_buffer);

		/* create v4l2 ctrl for be-soc video node */
		rval =
		  v4l2_ctrl_handler_init(&csi2_be_soc->av[i].ctrl_handler, 0);
		if (rval) {
			dev_err(&isys->adev->dev,
				"failed to init v4l2 ctrl hdl for be_soc\n");
			goto fail;
		}

		csi2_be_soc->av[i].compression_ctrl =
			v4l2_ctrl_new_custom(&csi2_be_soc->av[i].ctrl_handler,
					     &compression_ctrl_cfg, NULL);
		if (!csi2_be_soc->av[i].compression_ctrl) {
			dev_err(&isys->adev->dev,
				"failed to create BE-SOC cmprs ctrl\n");
			goto fail;
		}
		csi2_be_soc->av[i].compression = 0;
		csi2_be_soc->av[i].vdev.ctrl_handler =
			&csi2_be_soc->av[i].ctrl_handler;

		rval = ipu_isys_video_init(&csi2_be_soc->av[i],
					   &csi2_be_soc->asd.sd.entity,
					   CSI2_BE_SOC_PAD_SOURCE(i),
					   MEDIA_PAD_FL_SINK,
					   MEDIA_LNK_FL_DYNAMIC);
		if (rval) {
			dev_info(&isys->adev->dev, "can't init video node\n");
			goto fail;
		}
	}

	return 0;

fail:
	ipu_isys_csi2_be_soc_cleanup(csi2_be_soc);

	return rval;
}
