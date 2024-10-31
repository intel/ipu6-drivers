// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2013 - 2024 Intel Corporation

#include <linux/types.h>
#include <linux/videodev2.h>

#include <media/media-entity.h>

#include <uapi/linux/media-bus-format.h>

#include "ipu-isys.h"
#include "ipu-isys-video.h"
#include "ipu-isys-subdev.h"

unsigned int ipu_isys_mbus_code_to_bpp(u32 code)
{
	switch (code) {
	case MEDIA_BUS_FMT_RGB888_1X24:
		return 24;
#ifdef V4L2_PIX_FMT_Y210
	case MEDIA_BUS_FMT_YUYV10_1X20:
		return 20;
#endif
	case MEDIA_BUS_FMT_Y10_1X10:
	case MEDIA_BUS_FMT_RGB565_1X16:
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_VYUY8_1X16:
		return 16;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		return 12;
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		return 10;
	case MEDIA_BUS_FMT_Y8_1X8:
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_SBGGR10_DPCM8_1X8:
	case MEDIA_BUS_FMT_SGBRG10_DPCM8_1X8:
	case MEDIA_BUS_FMT_SGRBG10_DPCM8_1X8:
	case MEDIA_BUS_FMT_SRGGB10_DPCM8_1X8:
		return 8;
	default:
		WARN_ON(1);
		return -EINVAL;
	}
}

unsigned int ipu_isys_mbus_code_to_mipi(u32 code)
{
	switch (code) {
	case MEDIA_BUS_FMT_RGB565_1X16:
		return IPU_ISYS_MIPI_CSI2_TYPE_RGB565;
	case MEDIA_BUS_FMT_RGB888_1X24:
		return IPU_ISYS_MIPI_CSI2_TYPE_RGB888;
#ifdef V4L2_PIX_FMT_Y210
	case MEDIA_BUS_FMT_YUYV10_1X20:
		return IPU_ISYS_MIPI_CSI2_TYPE_YUV422_10;
#endif
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_VYUY8_1X16:
		return IPU_ISYS_MIPI_CSI2_TYPE_YUV422_8;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		return IPU_ISYS_MIPI_CSI2_TYPE_RAW12;
	case MEDIA_BUS_FMT_Y10_1X10:
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		return IPU_ISYS_MIPI_CSI2_TYPE_RAW10;
	case MEDIA_BUS_FMT_Y8_1X8:
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		return IPU_ISYS_MIPI_CSI2_TYPE_RAW8;
	case MEDIA_BUS_FMT_SBGGR10_DPCM8_1X8:
	case MEDIA_BUS_FMT_SGBRG10_DPCM8_1X8:
	case MEDIA_BUS_FMT_SGRBG10_DPCM8_1X8:
	case MEDIA_BUS_FMT_SRGGB10_DPCM8_1X8:
		return IPU_ISYS_MIPI_CSI2_TYPE_USER_DEF(1);
	default:
		WARN_ON(1);
		return -EINVAL;
	}
}

enum ipu_isys_subdev_pixelorder ipu_isys_subdev_get_pixelorder(u32 code)
{
	switch (code) {
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SBGGR10_DPCM8_1X8:
		return IPU_ISYS_SUBDEV_PIXELORDER_BGGR;
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGBRG10_DPCM8_1X8:
		return IPU_ISYS_SUBDEV_PIXELORDER_GBRG;
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SGRBG10_DPCM8_1X8:
		return IPU_ISYS_SUBDEV_PIXELORDER_GRBG;
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_SRGGB10_DPCM8_1X8:
		return IPU_ISYS_SUBDEV_PIXELORDER_RGGB;
	default:
		WARN_ON(1);
		return -EINVAL;
	}
}

u32 ipu_isys_subdev_code_to_uncompressed(u32 sink_code)
{
	switch (sink_code) {
	case MEDIA_BUS_FMT_SBGGR10_DPCM8_1X8:
		return MEDIA_BUS_FMT_SBGGR10_1X10;
	case MEDIA_BUS_FMT_SGBRG10_DPCM8_1X8:
		return MEDIA_BUS_FMT_SGBRG10_1X10;
	case MEDIA_BUS_FMT_SGRBG10_DPCM8_1X8:
		return MEDIA_BUS_FMT_SGRBG10_1X10;
	case MEDIA_BUS_FMT_SRGGB10_DPCM8_1X8:
		return MEDIA_BUS_FMT_SRGGB10_1X10;
	default:
		return sink_code;
	}
}

struct v4l2_mbus_framefmt *__ipu_isys_get_ffmt(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
					       struct v4l2_subdev_fh *cfg,
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
					       struct v4l2_subdev_pad_config
					       *cfg,
#else
					       struct v4l2_subdev_state *state,
#endif
					       unsigned int pad,
					       unsigned int which)
{
	struct ipu_isys_subdev *asd = to_ipu_isys_subdev(sd);

	if (which == V4L2_SUBDEV_FORMAT_ACTIVE)
		return &asd->ffmt[pad];
	else
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
		return v4l2_subdev_get_try_format(cfg, pad);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		return v4l2_subdev_get_try_format(sd, cfg, pad);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
		return v4l2_subdev_get_try_format(sd, state, pad);
#else
		return v4l2_subdev_state_get_format(state, pad);
#endif
}

struct v4l2_rect *__ipu_isys_get_selection(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
					   struct v4l2_subdev_fh *cfg,
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
					   struct v4l2_subdev_pad_config *cfg,
#else
					   struct v4l2_subdev_state *state,
#endif
					   unsigned int target,
					   unsigned int pad, unsigned int which)
{
	struct ipu_isys_subdev *asd = to_ipu_isys_subdev(sd);

	if (which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		switch (target) {
		case V4L2_SEL_TGT_CROP:
			return &asd->crop[pad];
		case V4L2_SEL_TGT_COMPOSE:
			return &asd->compose[pad];
		}
	} else {
		switch (target) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
		case V4L2_SEL_TGT_CROP:
			return v4l2_subdev_get_try_crop(cfg, pad);
		case V4L2_SEL_TGT_COMPOSE:
			return v4l2_subdev_get_try_compose(cfg, pad);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		case V4L2_SEL_TGT_CROP:
			return v4l2_subdev_get_try_crop(sd, cfg, pad);
		case V4L2_SEL_TGT_COMPOSE:
			return v4l2_subdev_get_try_compose(sd, cfg, pad);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
		case V4L2_SEL_TGT_CROP:
			return v4l2_subdev_get_try_crop(sd, state, pad);
		case V4L2_SEL_TGT_COMPOSE:
			return v4l2_subdev_get_try_compose(sd, state, pad);
#else
		case V4L2_SEL_TGT_CROP:
			return v4l2_subdev_state_get_crop(state, pad);
		case V4L2_SEL_TGT_COMPOSE:
			return v4l2_subdev_state_get_compose(state, pad);
#endif
		}
	}
	WARN_ON(1);
	return NULL;
}

static int target_valid(struct v4l2_subdev *sd, unsigned int target,
			unsigned int pad)
{
	struct ipu_isys_subdev *asd = to_ipu_isys_subdev(sd);

	switch (target) {
	case V4L2_SEL_TGT_CROP:
		return asd->valid_tgts[pad].crop;
	case V4L2_SEL_TGT_COMPOSE:
		return asd->valid_tgts[pad].compose;
	default:
		return 0;
	}
}

int ipu_isys_subdev_fmt_propagate(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
				  struct v4l2_subdev_fh *cfg,
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
				  struct v4l2_subdev_pad_config *cfg,
#else
				  struct v4l2_subdev_state *state,
#endif
				  struct v4l2_mbus_framefmt *ffmt,
				  struct v4l2_rect *r,
				  enum isys_subdev_prop_tgt tgt,
				  unsigned int pad, unsigned int which)
{
	struct ipu_isys_subdev *asd = to_ipu_isys_subdev(sd);
	struct v4l2_mbus_framefmt **ffmts = NULL;
	struct v4l2_rect **crops = NULL;
	struct v4l2_rect **compose = NULL;
	unsigned int i;
	int rval = 0;

	if (tgt == IPU_ISYS_SUBDEV_PROP_TGT_NR_OF)
		return 0;

	if (WARN_ON(pad >= sd->entity.num_pads))
		return -EINVAL;

	ffmts = kcalloc(sd->entity.num_pads,
			sizeof(*ffmts), GFP_KERNEL);
	if (!ffmts) {
		rval = -ENOMEM;
		goto out_subdev_fmt_propagate;
	}
	crops = kcalloc(sd->entity.num_pads,
			sizeof(*crops), GFP_KERNEL);
	if (!crops) {
		rval = -ENOMEM;
		goto out_subdev_fmt_propagate;
	}
	compose = kcalloc(sd->entity.num_pads,
			  sizeof(*compose), GFP_KERNEL);
	if (!compose) {
		rval = -ENOMEM;
		goto out_subdev_fmt_propagate;
	}

	for (i = 0; i < sd->entity.num_pads; i++) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		ffmts[i] = __ipu_isys_get_ffmt(sd, cfg, i, which);
		crops[i] = __ipu_isys_get_selection(sd, cfg, V4L2_SEL_TGT_CROP,
						    i, which);
		compose[i] = __ipu_isys_get_selection(sd, cfg,
						      V4L2_SEL_TGT_COMPOSE,
						      i, which);
#else
		ffmts[i] = __ipu_isys_get_ffmt(sd, state, i, which);
		crops[i] = __ipu_isys_get_selection(sd, state, V4L2_SEL_TGT_CROP,
						    i, which);
		compose[i] = __ipu_isys_get_selection(sd, state,
						      V4L2_SEL_TGT_COMPOSE,
						      i, which);
#endif
	}

	switch (tgt) {
	case IPU_ISYS_SUBDEV_PROP_TGT_SINK_FMT:
		crops[pad]->left = 0;
		crops[pad]->top = 0;
		crops[pad]->width = ffmt->width;
		crops[pad]->height = ffmt->height;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		rval = ipu_isys_subdev_fmt_propagate(sd, cfg, ffmt, crops[pad],
						     tgt + 1, pad, which);
#else
		rval = ipu_isys_subdev_fmt_propagate(sd, state, ffmt, crops[pad],
						     tgt + 1, pad, which);
#endif
		goto out_subdev_fmt_propagate;
	case IPU_ISYS_SUBDEV_PROP_TGT_SINK_CROP:
		if (WARN_ON(sd->entity.pads[pad].flags & MEDIA_PAD_FL_SOURCE))
			goto out_subdev_fmt_propagate;

		compose[pad]->left = 0;
		compose[pad]->top = 0;
		compose[pad]->width = r->width;
		compose[pad]->height = r->height;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		rval = ipu_isys_subdev_fmt_propagate(sd, cfg, ffmt,
						     compose[pad], tgt + 1,
						     pad, which);
#else
		rval = ipu_isys_subdev_fmt_propagate(sd, state, ffmt,
						     compose[pad], tgt + 1,
						     pad, which);
#endif
		goto out_subdev_fmt_propagate;
	case IPU_ISYS_SUBDEV_PROP_TGT_SINK_COMPOSE:
		if (WARN_ON(sd->entity.pads[pad].flags & MEDIA_PAD_FL_SOURCE)) {
			rval = -EINVAL;
			goto out_subdev_fmt_propagate;
		}

		for (i = 1; i < sd->entity.num_pads; i++) {
			if (!(sd->entity.pads[i].flags &
					MEDIA_PAD_FL_SOURCE))
				continue;

			compose[i]->left = 0;
			compose[i]->top = 0;
			compose[i]->width = r->width;
			compose[i]->height = r->height;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			rval = ipu_isys_subdev_fmt_propagate(sd, cfg,
							     ffmt,
							     compose[i],
							     tgt + 1, i,
							     which);
#else
			rval = ipu_isys_subdev_fmt_propagate(sd, state,
							     ffmt,
							     compose[i],
							     tgt + 1, i,
							     which);
#endif
			if (rval)
				goto out_subdev_fmt_propagate;
		}
		goto out_subdev_fmt_propagate;
	case IPU_ISYS_SUBDEV_PROP_TGT_SOURCE_COMPOSE:
		if (WARN_ON(sd->entity.pads[pad].flags & MEDIA_PAD_FL_SINK)) {
			rval = -EINVAL;
			goto out_subdev_fmt_propagate;
		}

		crops[pad]->left = 0;
		crops[pad]->top = 0;
		crops[pad]->width = r->width;
		crops[pad]->height = r->height;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		rval = ipu_isys_subdev_fmt_propagate(sd, cfg, ffmt,
						     crops[pad], tgt + 1,
						     pad, which);
#else
		rval = ipu_isys_subdev_fmt_propagate(sd, state, ffmt,
						     crops[pad], tgt + 1,
						     pad, which);
#endif
		goto out_subdev_fmt_propagate;
	case IPU_ISYS_SUBDEV_PROP_TGT_SOURCE_CROP:{
			struct v4l2_subdev_format fmt = {
				.which = which,
				.pad = pad,
				.format = {
					.width = r->width,
					.height = r->height,
					/*
					 * Either use the code from sink pad
					 * or the current one.
					 */
					.code = ffmt ? ffmt->code :
						       ffmts[pad]->code,
					.field = ffmt ? ffmt->field :
							ffmts[pad]->field,
				},
			};

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			asd->set_ffmt(sd, cfg, &fmt);
#else
			asd->set_ffmt(sd, state, &fmt);
#endif
			goto out_subdev_fmt_propagate;
		}
	}

out_subdev_fmt_propagate:
	kfree(ffmts);
	kfree(crops);
	kfree(compose);
	return rval;
}

int ipu_isys_subdev_set_ffmt_default(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
				     struct v4l2_subdev_fh *cfg,
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
				     struct v4l2_subdev_pad_config *cfg,
#else
				     struct v4l2_subdev_state *state,
#endif
				     struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *ffmt =
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		__ipu_isys_get_ffmt(sd, cfg, fmt->pad, fmt->which);
#else
		__ipu_isys_get_ffmt(sd, state, fmt->pad, fmt->which);
#endif

	/* No propagation for non-zero pads. */
	if (fmt->pad) {
		struct v4l2_mbus_framefmt *sink_ffmt =
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			__ipu_isys_get_ffmt(sd, cfg, 0, fmt->which);
#else
			__ipu_isys_get_ffmt(sd, state, 0, fmt->which);
#endif

		ffmt->width = sink_ffmt->width;
		ffmt->height = sink_ffmt->height;
		ffmt->code = sink_ffmt->code;
		ffmt->field = sink_ffmt->field;

		return 0;
	}

	ffmt->width = fmt->format.width;
	ffmt->height = fmt->format.height;
	ffmt->code = fmt->format.code;
	ffmt->field = fmt->format.field;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
	return ipu_isys_subdev_fmt_propagate(sd, cfg, &fmt->format, NULL,
					     IPU_ISYS_SUBDEV_PROP_TGT_SINK_FMT,
					     fmt->pad, fmt->which);
#else
	return ipu_isys_subdev_fmt_propagate(sd, state, &fmt->format, NULL,
					     IPU_ISYS_SUBDEV_PROP_TGT_SINK_FMT,
					     fmt->pad, fmt->which);
#endif
}

int __ipu_isys_subdev_set_ffmt(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
			       struct v4l2_subdev_fh *cfg,
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			       struct v4l2_subdev_pad_config *cfg,
#else
			       struct v4l2_subdev_state *state,
#endif
			       struct v4l2_subdev_format *fmt)
{
	struct ipu_isys_subdev *asd = to_ipu_isys_subdev(sd);
	struct v4l2_mbus_framefmt *ffmt =
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		__ipu_isys_get_ffmt(sd, cfg, fmt->pad, fmt->which);
#else
		__ipu_isys_get_ffmt(sd, state, fmt->pad, fmt->which);
#endif
	u32 code = asd->supported_codes[fmt->pad][0];
	unsigned int i;

	WARN_ON(!mutex_is_locked(&asd->mutex));

	fmt->format.width = clamp(fmt->format.width, IPU_ISYS_MIN_WIDTH,
				  IPU_ISYS_MAX_WIDTH);
	fmt->format.height = clamp(fmt->format.height,
				   IPU_ISYS_MIN_HEIGHT, IPU_ISYS_MAX_HEIGHT);

	for (i = 0; asd->supported_codes[fmt->pad][i]; i++) {
		if (asd->supported_codes[fmt->pad][i] == fmt->format.code) {
			code = asd->supported_codes[fmt->pad][i];
			break;
		}
	}

	fmt->format.code = code;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
	asd->set_ffmt(sd, cfg, fmt);
#else
	asd->set_ffmt(sd, state, fmt);
#endif

	fmt->format = *ffmt;

	return 0;
}

int ipu_isys_subdev_set_ffmt(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
			     struct v4l2_subdev_fh *cfg,
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			     struct v4l2_subdev_pad_config *cfg,
#else
			     struct v4l2_subdev_state *state,
#endif
			     struct v4l2_subdev_format *fmt)
{
	struct ipu_isys_subdev *asd = to_ipu_isys_subdev(sd);
	int rval;

	mutex_lock(&asd->mutex);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
	rval = __ipu_isys_subdev_set_ffmt(sd, cfg, fmt);
#else
	rval = __ipu_isys_subdev_set_ffmt(sd, state, fmt);
#endif
	mutex_unlock(&asd->mutex);

	return rval;
}

int ipu_isys_subdev_get_ffmt(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
			     struct v4l2_subdev_fh *cfg,
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			     struct v4l2_subdev_pad_config *cfg,
#else
			     struct v4l2_subdev_state *state,
#endif
			     struct v4l2_subdev_format *fmt)
{
	struct ipu_isys_subdev *asd = to_ipu_isys_subdev(sd);

	mutex_lock(&asd->mutex);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
	fmt->format = *__ipu_isys_get_ffmt(sd, cfg, fmt->pad,
					   fmt->which);
#else
	fmt->format = *__ipu_isys_get_ffmt(sd, state, fmt->pad,
					   fmt->which);
#endif
	mutex_unlock(&asd->mutex);

	return 0;
}

int ipu_isys_subdev_set_sel(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
			    struct v4l2_subdev_fh *cfg,
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			    struct v4l2_subdev_pad_config *cfg,
#else
			    struct v4l2_subdev_state *state,
#endif
			    struct v4l2_subdev_selection *sel)
{
	struct ipu_isys_subdev *asd = to_ipu_isys_subdev(sd);
	struct media_pad *pad = &asd->sd.entity.pads[sel->pad];
	struct v4l2_rect *r, __r = { 0 };
	unsigned int tgt;

	if (!target_valid(sd, sel->target, sel->pad))
		return -EINVAL;

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		if (pad->flags & MEDIA_PAD_FL_SINK) {
			struct v4l2_mbus_framefmt *ffmt =
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
				__ipu_isys_get_ffmt(sd, cfg, sel->pad,
						    sel->which);
#else
				__ipu_isys_get_ffmt(sd, state, sel->pad,
						    sel->which);
#endif

			__r.width = ffmt->width;
			__r.height = ffmt->height;
			r = &__r;
			tgt = IPU_ISYS_SUBDEV_PROP_TGT_SINK_CROP;
		} else {
			/* 0 is the sink pad. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			r = __ipu_isys_get_selection(sd, cfg, sel->target, 0,
						     sel->which);
#else
			r = __ipu_isys_get_selection(sd, state, sel->target, 0,
						     sel->which);
#endif
			tgt = IPU_ISYS_SUBDEV_PROP_TGT_SOURCE_CROP;
		}

		break;
	case V4L2_SEL_TGT_COMPOSE:
		if (pad->flags & MEDIA_PAD_FL_SINK) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			r = __ipu_isys_get_selection(sd, cfg, V4L2_SEL_TGT_CROP,
						     sel->pad, sel->which);
#else
			r = __ipu_isys_get_selection(sd, state, V4L2_SEL_TGT_CROP,
						     sel->pad, sel->which);
#endif
			tgt = IPU_ISYS_SUBDEV_PROP_TGT_SINK_COMPOSE;
		} else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			r = __ipu_isys_get_selection(sd, cfg,
						     V4L2_SEL_TGT_COMPOSE, 0,
						     sel->which);
#else
			r = __ipu_isys_get_selection(sd, state,
						     V4L2_SEL_TGT_COMPOSE, 0,
						     sel->which);
#endif
			tgt = IPU_ISYS_SUBDEV_PROP_TGT_SOURCE_COMPOSE;
		}
		break;
	default:
		return -EINVAL;
	}

	sel->r.width = clamp(sel->r.width, IPU_ISYS_MIN_WIDTH, r->width);
	sel->r.height = clamp(sel->r.height, IPU_ISYS_MIN_HEIGHT, r->height);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
	*__ipu_isys_get_selection(sd, cfg, sel->target, sel->pad,
				  sel->which) = sel->r;
	return ipu_isys_subdev_fmt_propagate(sd, cfg, NULL, &sel->r, tgt,
					     sel->pad, sel->which);
#else
	*__ipu_isys_get_selection(sd, state, sel->target, sel->pad,
				  sel->which) = sel->r;
	return ipu_isys_subdev_fmt_propagate(sd, state, NULL, &sel->r, tgt,
					     sel->pad, sel->which);
#endif
}

int ipu_isys_subdev_get_sel(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
			    struct v4l2_subdev_fh *cfg,
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
			    struct v4l2_subdev_pad_config *cfg,
#else
			    struct v4l2_subdev_state *state,
#endif
			    struct v4l2_subdev_selection *sel)
{
	if (!target_valid(sd, sel->target, sel->pad))
		return -EINVAL;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
	sel->r = *__ipu_isys_get_selection(sd, cfg, sel->target,
					   sel->pad, sel->which);
#else
	sel->r = *__ipu_isys_get_selection(sd, state, sel->target,
					   sel->pad, sel->which);
#endif

	return 0;
}

int ipu_isys_subdev_enum_mbus_code(struct v4l2_subdev *sd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
				   struct v4l2_subdev_fh *cfg,
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
				   struct v4l2_subdev_pad_config *cfg,
#else
				   struct v4l2_subdev_state *state,
#endif
				   struct v4l2_subdev_mbus_code_enum *code)
{
	struct ipu_isys_subdev *asd = to_ipu_isys_subdev(sd);
	const u32 *supported_codes = asd->supported_codes[code->pad];
	u32 index;

	for (index = 0; supported_codes[index]; index++) {
		if (index == code->index) {
			code->code = supported_codes[index];
			return 0;
		}
	}

	return -EINVAL;
}

/*
 * Besides validating the link, figure out the external pad and the
 * ISYS FW ABI source.
 */
int ipu_isys_subdev_link_validate(struct v4l2_subdev *sd,
				  struct media_link *link,
				  struct v4l2_subdev_format *source_fmt,
				  struct v4l2_subdev_format *sink_fmt)
{
	struct v4l2_subdev *source_sd =
	    media_entity_to_v4l2_subdev(link->source->entity);
	struct media_pipeline *mp = media_entity_pipeline(&sd->entity);
	struct ipu_isys_pipeline *ip = container_of(mp,
						    struct ipu_isys_pipeline,
						    pipe);
	struct ipu_isys_subdev *asd = to_ipu_isys_subdev(sd);

	if (!source_sd)
		return -ENODEV;
	if (strncmp(source_sd->name, IPU_ISYS_ENTITY_PREFIX,
		    strlen(IPU_ISYS_ENTITY_PREFIX)) != 0) {
		/*
		 * source_sd isn't ours --- sd must be the external
		 * sub-device.
		 */
		ip->external = link->source;
		ip->source = to_ipu_isys_subdev(sd)->source;
		dev_dbg(&asd->isys->adev->dev, "%s: using source %d\n",
			sd->entity.name, ip->source);
		/*
		 * multi streams with different format/resolusion from external,
		 * without route info, ignore link validate here.
		 */
		return 0;
	} else if (source_sd->entity.num_pads == 1) {
		/* All internal sources have a single pad. */
		ip->external = link->source;
		ip->source = to_ipu_isys_subdev(source_sd)->source;

		dev_dbg(&asd->isys->adev->dev, "%s: using source %d\n",
			sd->entity.name, ip->source);
	}

	if (asd->isl_mode != IPU_ISL_OFF)
		ip->isl_mode = asd->isl_mode;

	return v4l2_subdev_link_validate_default(sd, link, source_fmt,
						 sink_fmt);
}

int ipu_isys_subdev_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ipu_isys_subdev *asd = to_ipu_isys_subdev(sd);
	unsigned int i;

	mutex_lock(&asd->mutex);

	for (i = 0; i < asd->sd.entity.num_pads; i++) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
		struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(fh, i);
		struct v4l2_rect *try_crop =
			v4l2_subdev_get_try_crop(fh, i);
		struct v4l2_rect *try_compose =
			v4l2_subdev_get_try_compose(fh, i);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
		struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(sd, fh->pad, i);
		struct v4l2_rect *try_crop =
			v4l2_subdev_get_try_crop(sd, fh->pad, i);
		struct v4l2_rect *try_compose =
			v4l2_subdev_get_try_compose(sd, fh->pad, i);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
		struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(sd, fh->state, i);
		struct v4l2_rect *try_crop =
			v4l2_subdev_get_try_crop(sd, fh->state, i);
		struct v4l2_rect *try_compose =
			v4l2_subdev_get_try_compose(sd, fh->state, i);
#else
		struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_state_get_format(fh->state, i);
		struct v4l2_rect *try_crop =
			v4l2_subdev_state_get_crop(fh->state, i);
		struct v4l2_rect *try_compose =
			v4l2_subdev_state_get_compose(fh->state, i);
#endif

		*try_fmt = asd->ffmt[i];
		*try_crop = asd->crop[i];
		*try_compose = asd->compose[i];
	}

	mutex_unlock(&asd->mutex);

	return 0;
}

int ipu_isys_subdev_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	return 0;
}

int ipu_isys_subdev_init(struct ipu_isys_subdev *asd,
			 struct v4l2_subdev_ops *ops,
			 unsigned int nr_ctrls,
			 unsigned int num_pads,
			 unsigned int num_source,
			 unsigned int num_sink,
			 unsigned int sd_flags,
			 int src_pad_idx,
			 int sink_pad_idx)
{
	unsigned int i;
	int rval = -EINVAL;

	mutex_init(&asd->mutex);

	v4l2_subdev_init(&asd->sd, ops);

	asd->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | sd_flags;
	asd->sd.owner = THIS_MODULE;
	asd->sd.entity.function = MEDIA_ENT_F_VID_IF_BRIDGE;

	asd->nsources = num_source;
	asd->nsinks = num_sink;

	asd->pad = devm_kcalloc(&asd->isys->adev->dev, num_pads,
				sizeof(*asd->pad), GFP_KERNEL);

	/*
	 * Out of range IDX means that this particular type of pad
	 * does not exist.
	 */
	if (src_pad_idx != ISYS_SUBDEV_NO_PAD) {
		for (i = 0; i < num_source; i++)
			asd->pad[src_pad_idx + i].flags = MEDIA_PAD_FL_SOURCE;
	}
	if (sink_pad_idx != ISYS_SUBDEV_NO_PAD) {
		for (i = 0; i < num_sink; i++)
			asd->pad[sink_pad_idx + i].flags = MEDIA_PAD_FL_SINK;
	}

	asd->ffmt = devm_kcalloc(&asd->isys->adev->dev, num_pads,
				 sizeof(*asd->ffmt), GFP_KERNEL);

	asd->crop = devm_kcalloc(&asd->isys->adev->dev, num_pads,
				 sizeof(*asd->crop), GFP_KERNEL);

	asd->compose = devm_kcalloc(&asd->isys->adev->dev, num_pads,
				    sizeof(*asd->compose), GFP_KERNEL);

	asd->valid_tgts = devm_kcalloc(&asd->isys->adev->dev, num_pads,
				       sizeof(*asd->valid_tgts), GFP_KERNEL);
	if (!asd->pad || !asd->ffmt || !asd->crop || !asd->compose ||
	    !asd->valid_tgts)
		return -ENOMEM;

	rval = media_entity_pads_init(&asd->sd.entity, num_pads, asd->pad);
	if (rval)
		goto out_mutex_destroy;

	if (asd->ctrl_init) {
		rval = v4l2_ctrl_handler_init(&asd->ctrl_handler, nr_ctrls);
		if (rval)
			goto out_media_entity_cleanup;

		asd->ctrl_init(&asd->sd);
		if (asd->ctrl_handler.error) {
			rval = asd->ctrl_handler.error;
			goto out_v4l2_ctrl_handler_free;
		}

		asd->sd.ctrl_handler = &asd->ctrl_handler;
	}

	asd->source = -1;

	return 0;

out_v4l2_ctrl_handler_free:
	v4l2_ctrl_handler_free(&asd->ctrl_handler);

out_media_entity_cleanup:
	media_entity_cleanup(&asd->sd.entity);

out_mutex_destroy:
	mutex_destroy(&asd->mutex);

	return rval;
}

void ipu_isys_subdev_cleanup(struct ipu_isys_subdev *asd)
{
	media_entity_cleanup(&asd->sd.entity);
	v4l2_ctrl_handler_free(&asd->ctrl_handler);
	mutex_destroy(&asd->mutex);
}
