/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2013 - 2024 Intel Corporation */

#ifndef IPU_ISYS_VIDEO_H
#define IPU_ISYS_VIDEO_H

#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/videodev2.h>
#include <media/media-entity.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include "ipu-isys-queue.h"
#include "ipu-platform-isys.h"

#define IPU_ISYS_OUTPUT_PINS 11
#define IPU_NUM_CAPTURE_DONE 2
#define IPU_ISYS_MAX_PARALLEL_SOF 2
#define CSI2_BE_SOC_SOURCE_PADS_NUM NR_OF_CSI2_BE_SOC_STREAMS

struct ipu_isys;
struct ipu_isys_csi2_be_soc;
struct ipu_fw_isys_stream_cfg_data_abi;

struct ipu_isys_pixelformat {
	u32 pixelformat;
	u32 bpp;
	u32 bpp_packed;
	u32 bpp_planar;
	u32 code;
	u32 css_pixelformat;
};

struct sequence_info {
	unsigned int sequence;
	u64 timestamp;
};

struct output_pin_data {
	void (*pin_ready)(struct ipu_isys_pipeline *ip,
			  struct ipu_fw_isys_resp_info_abi *info);
	struct ipu_isys_queue *aq;
};

/*
 * struct ipu_isys_sub_stream_vc
 */
struct ipu_isys_sub_stream_vc {
	unsigned int substream;	/* sub stream id */
	int vc;	/* VC number */
	u32 width;
	u32 height;
	unsigned int dt;
	unsigned int code;
};

#define SUB_STREAM_CODE(value) ((value) & 0xFFFF)
#define SUB_STREAM_H(value) (((value) >> 16) & 0xFFFF)
#define SUB_STREAM_W(value) (((value) >> 32) & 0xFFFF)
#define SUB_STREAM_DT(value) (((value) >> 48) & 0xFF)
#define SUB_STREAM_VC_ID(value) ((value) >> 56 & 0xFF)
#define SUB_STREAM_SET_VALUE(vc_id, stream_state) \
	((((vc_id) << 8) & 0xFF00) | (stream_state))

struct ipu_isys_pipeline {
	struct media_pipeline pipe;
	struct media_pad *external;
	atomic_t sequence;
	int last_sequence;
	unsigned int seq_index;
	struct sequence_info seq[IPU_ISYS_MAX_PARALLEL_SOF];
	int source;	/* SSI stream source */
	int stream_handle;	/* stream handle for CSS API */
	unsigned int nr_output_pins;	/* How many firmware pins? */
	enum ipu_isl_mode isl_mode;
	struct ipu_isys_csi2_be *csi2_be;
	struct ipu_isys_csi2_be_soc *csi2_be_soc;
	struct ipu_isys_csi2 *csi2;

	/*
	 * Number of capture queues, write access serialised using struct
	 * ipu_isys.stream_mutex
	 */
	/* If it supports vc, this is number of links for the same vc. */
	int nr_queues;
	int nr_streaming;	/* Number of capture queues streaming */
	int streaming;	/* Has streaming been really started? */
	struct list_head queues;
	struct completion stream_open_completion;
	struct completion stream_close_completion;
	struct completion stream_start_completion;
	struct completion stream_stop_completion;
	struct ipu_isys *isys;

	void (*capture_done[IPU_NUM_CAPTURE_DONE])
	 (struct ipu_isys_pipeline *ip,
	  struct ipu_fw_isys_resp_info_abi *resp);
	struct output_pin_data output_pins[IPU_ISYS_OUTPUT_PINS];
	bool has_sof;
	bool interlaced;
	int error;
	struct ipu_isys_private_buffer *short_packet_bufs;
	size_t short_packet_buffer_size;
	unsigned int num_short_packet_lines;
	unsigned int short_packet_output_pin;
	unsigned int cur_field;
	struct list_head short_packet_incoming;
	struct list_head short_packet_active;
	/* Serialize access to short packet active and incoming lists */
	spinlock_t short_packet_queue_lock;
	struct list_head pending_interlaced_bufs;
	unsigned int short_packet_trace_index;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	struct media_graph graph;
#else
	struct media_entity_graph graph;
#endif
#endif
	struct media_entity_enum entity_enum;
	unsigned int vc;
	struct ipu_isys_sub_stream_vc asv[CSI2_BE_SOC_SOURCE_PADS_NUM];
};

#define to_ipu_isys_pipeline(__pipe)				\
	container_of((__pipe), struct ipu_isys_pipeline, pipe)

struct ipu_isys_video {
	/* Serialise access to other fields in the struct. */
	struct mutex mutex;
	struct media_pad pad;
	struct video_device vdev;
	struct v4l2_pix_format_mplane mpix;
	const struct ipu_isys_pixelformat *pfmts;
	const struct ipu_isys_pixelformat *pfmt;
	struct ipu_isys_queue aq;
	struct ipu_isys *isys;
	struct ipu_isys_pipeline ip;
	unsigned int streaming;
	unsigned int reset;
	unsigned int skipframe;
	unsigned int start_streaming;
	bool packed;
	bool compression;
	bool initialized;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *compression_ctrl;
	unsigned int ts_offsets[VIDEO_MAX_PLANES];
	unsigned int line_header_length;	/* bits */
	unsigned int line_footer_length;	/* bits */

	const struct ipu_isys_pixelformat *
		(*try_fmt_vid_mplane)(struct ipu_isys_video *av,
				      struct v4l2_pix_format_mplane *mpix);
	void (*prepare_fw_stream)(struct ipu_isys_video *av,
				  struct ipu_fw_isys_stream_cfg_data_abi *cfg);
};

#define ipu_isys_queue_to_video(__aq) \
	container_of(__aq, struct ipu_isys_video, aq)

extern const struct ipu_isys_pixelformat ipu_isys_pfmts[];
extern const struct ipu_isys_pixelformat ipu_isys_pfmts_be_soc[];
extern const struct ipu_isys_pixelformat ipu_isys_pfmts_packed[];

const struct ipu_isys_pixelformat *
ipu_isys_get_pixelformat(struct ipu_isys_video *av, u32 pixelformat);

int ipu_isys_vidioc_querycap(struct file *file, void *fh,
			     struct v4l2_capability *cap);

int ipu_isys_vidioc_enum_fmt(struct file *file, void *fh,
			     struct v4l2_fmtdesc *f);

const struct ipu_isys_pixelformat *
ipu_isys_video_try_fmt_vid_mplane_default(struct ipu_isys_video *av,
					  struct v4l2_pix_format_mplane *mpix);

const struct ipu_isys_pixelformat *
ipu_isys_video_try_fmt_vid_mplane(struct ipu_isys_video *av,
				  struct v4l2_pix_format_mplane *mpix,
				  int store_csi2_header);

void
ipu_isys_prepare_fw_cfg_default(struct ipu_isys_video *av,
				struct ipu_fw_isys_stream_cfg_data_abi *cfg);
int ipu_isys_video_prepare_streaming(struct ipu_isys_video *av,
				     unsigned int state);
int ipu_isys_video_set_streaming(struct ipu_isys_video *av, unsigned int state,
				 struct ipu_isys_buffer_list *bl);
int ipu_isys_video_init(struct ipu_isys_video *av, struct media_entity *source,
			unsigned int source_pad, unsigned long pad_flags,
			unsigned int flags);
void ipu_isys_video_cleanup(struct ipu_isys_video *av);
void ipu_isys_video_add_capture_done(struct ipu_isys_pipeline *ip,
				     void (*capture_done)
				      (struct ipu_isys_pipeline *ip,
				       struct ipu_fw_isys_resp_info_abi *resp));

bool is_support_vc(struct media_pad *source_pad,
						struct ipu_isys_pipeline *ip);

bool is_has_metadata(const struct ipu_isys_pipeline *ip);

#endif /* IPU_ISYS_VIDEO_H */
