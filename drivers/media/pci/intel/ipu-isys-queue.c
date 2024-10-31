// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2013 - 2024 Intel Corporation

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/delay.h>

#include <media/media-entity.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-ioctl.h>

#include "ipu.h"
#include "ipu-bus.h"
#include "ipu-cpd.h"
#include "ipu-platform-isys-csi2-reg.h"
#include "ipu-buttress.h"
#include "ipu-isys.h"
#include "ipu-isys-csi2.h"
#include "ipu-isys-video.h"

extern int vnode_num;

static bool wall_clock_ts_on;
module_param(wall_clock_ts_on, bool, 0660);
MODULE_PARM_DESC(wall_clock_ts_on, "Timestamp based on REALTIME clock");
extern bool enable_hw_sof_irq;

static int queue_setup(struct vb2_queue *q,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
		       const struct v4l2_format *__fmt,
#endif
		       unsigned int *num_buffers, unsigned int *num_planes,
		       unsigned int sizes[],
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
		       void *alloc_ctxs[])
#else
		       struct device *alloc_devs[])
#endif
{
	struct ipu_isys_queue *aq = vb2_queue_to_ipu_isys_queue(q);
	struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	const struct v4l2_format *fmt = __fmt;
	const struct ipu_isys_pixelformat *pfmt;
	struct v4l2_pix_format_mplane mpix;
#else
	bool use_fmt = false;
#endif
	unsigned int i;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	if (fmt)
		mpix = fmt->fmt.pix_mp;
	else
		mpix = av->mpix;

	pfmt = av->try_fmt_vid_mplane(av, &mpix);

	*num_planes = mpix.num_planes;
#else
	/* num_planes == 0: we're being called through VIDIOC_REQBUFS */
	if (!*num_planes) {
		use_fmt = true;
		*num_planes = av->mpix.num_planes;
	}
#endif

	for (i = 0; i < *num_planes; i++) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
		sizes[i] = mpix.plane_fmt[i].sizeimage;
#else
		if (use_fmt)
			sizes[i] = av->mpix.plane_fmt[i].sizeimage;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
		alloc_ctxs[i] = aq->ctx;
#else
		alloc_devs[i] = aq->dev;
#endif
		dev_dbg(&av->isys->adev->dev,
			"%s: queue setup: plane %d size %u\n",
			av->vdev.name, i, sizes[i]);
	}

	return 0;
}

static void ipu_isys_queue_lock(struct vb2_queue *q)
{
	struct ipu_isys_queue *aq = vb2_queue_to_ipu_isys_queue(q);
	struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);

	dev_dbg(&av->isys->adev->dev, "%s: queue lock\n", av->vdev.name);
	mutex_lock(&av->mutex);
}

static void ipu_isys_queue_unlock(struct vb2_queue *q)
{
	struct ipu_isys_queue *aq = vb2_queue_to_ipu_isys_queue(q);
	struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);

	dev_dbg(&av->isys->adev->dev, "%s: queue unlock\n", av->vdev.name);
	mutex_unlock(&av->mutex);
}

static int buf_init(struct vb2_buffer *vb)
{
	struct ipu_isys_queue *aq = vb2_queue_to_ipu_isys_queue(vb->vb2_queue);
	struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);

	dev_dbg(&av->isys->adev->dev, "buffer: %s: %s\n", av->vdev.name,
		__func__);

	if (aq->buf_init)
		return aq->buf_init(vb);

	return 0;
}

int ipu_isys_buf_prepare(struct vb2_buffer *vb)
{
	struct ipu_isys_queue *aq = vb2_queue_to_ipu_isys_queue(vb->vb2_queue);
	struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);

	dev_dbg(&av->isys->adev->dev,
		"buffer: %s: configured size %u, buffer size %lu\n",
		av->vdev.name,
		av->mpix.plane_fmt[0].sizeimage, vb2_plane_size(vb, 0));

	if (av->mpix.plane_fmt[0].sizeimage > vb2_plane_size(vb, 0))
		return -EINVAL;

	vb2_set_plane_payload(vb, 0, av->mpix.plane_fmt[0].bytesperline *
			      av->mpix.height);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	vb->v4l2_planes[0].data_offset = av->line_header_length / BITS_PER_BYTE;
#else
	vb->planes[0].data_offset = av->line_header_length / BITS_PER_BYTE;
#endif

	return 0;
}

static int buf_prepare(struct vb2_buffer *vb)
{
	struct ipu_isys_queue *aq = vb2_queue_to_ipu_isys_queue(vb->vb2_queue);
	struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);
	int rval;

	if (av->isys->adev->isp->flr_done)
		return -EIO;

	rval = aq->buf_prepare(vb);
	return rval;
}

static void buf_finish(struct vb2_buffer *vb)
{
	struct ipu_isys_queue *aq = vb2_queue_to_ipu_isys_queue(vb->vb2_queue);
	struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);

	dev_dbg(&av->isys->adev->dev, "buffer: %s: %s\n", av->vdev.name,
		__func__);

}

static void buf_cleanup(struct vb2_buffer *vb)
{
	struct ipu_isys_queue *aq = vb2_queue_to_ipu_isys_queue(vb->vb2_queue);
	struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);

	dev_dbg(&av->isys->adev->dev, "buffer: %s: %s\n", av->vdev.name,
		__func__);

	if (aq->buf_cleanup)
		aq->buf_cleanup(vb);
}

/*
 * Queue a buffer list back to incoming or active queues. The buffers
 * are removed from the buffer list.
 */
void ipu_isys_buffer_list_queue(struct ipu_isys_buffer_list *bl,
				unsigned long op_flags,
				enum vb2_buffer_state state)
{
	struct ipu_isys_buffer *ib, *ib_safe;
	unsigned long flags;
	bool first = true;

	if (!bl)
		return;

	WARN_ON(!bl->nbufs);
	WARN_ON(op_flags & IPU_ISYS_BUFFER_LIST_FL_ACTIVE &&
		op_flags & IPU_ISYS_BUFFER_LIST_FL_INCOMING);

	list_for_each_entry_safe(ib, ib_safe, &bl->head, head) {
		struct ipu_isys_video *av;

		if (ib->type == IPU_ISYS_VIDEO_BUFFER) {
			struct vb2_buffer *vb =
			    ipu_isys_buffer_to_vb2_buffer(ib);
			struct ipu_isys_queue *aq =
			    vb2_queue_to_ipu_isys_queue(vb->vb2_queue);

			av = ipu_isys_queue_to_video(aq);
			spin_lock_irqsave(&aq->lock, flags);
			list_del(&ib->head);
			if (op_flags & IPU_ISYS_BUFFER_LIST_FL_ACTIVE)
				list_add(&ib->head, &aq->active);
			else if (op_flags & IPU_ISYS_BUFFER_LIST_FL_INCOMING)
				list_add_tail(&ib->head, &aq->incoming);
			spin_unlock_irqrestore(&aq->lock, flags);

			if (op_flags & IPU_ISYS_BUFFER_LIST_FL_SET_STATE)
				vb2_buffer_done(vb, state);
		} else if (ib->type == IPU_ISYS_SHORT_PACKET_BUFFER) {
			struct ipu_isys_private_buffer *pb =
			    ipu_isys_buffer_to_private_buffer(ib);
			struct ipu_isys_pipeline *ip = pb->ip;

			av = container_of(ip, struct ipu_isys_video, ip);
			spin_lock_irqsave(&ip->short_packet_queue_lock, flags);
			list_del(&ib->head);
			if (op_flags & IPU_ISYS_BUFFER_LIST_FL_ACTIVE)
				list_add(&ib->head, &ip->short_packet_active);
			else if (op_flags & IPU_ISYS_BUFFER_LIST_FL_INCOMING)
				list_add(&ib->head, &ip->short_packet_incoming);
			spin_unlock_irqrestore(&ip->short_packet_queue_lock,
					       flags);
		} else {
			WARN_ON(1);
			return;
		}

		if (first) {
			dev_dbg(&av->isys->adev->dev,
				"queue buf list %p flags %lx, s %d, %d bufs\n",
				bl, op_flags, state, bl->nbufs);
			first = false;
		}

		bl->nbufs--;
	}

	WARN_ON(bl->nbufs);
}

/*
 * flush_firmware_streamon_fail() - Flush in cases where requests may
 * have been queued to firmware and the *firmware streamon fails for a
 * reason or another.
 */
static void flush_firmware_streamon_fail(struct ipu_isys_pipeline *ip)
{
	struct ipu_isys_video *pipe_av =
	    container_of(ip, struct ipu_isys_video, ip);
	struct ipu_isys_queue *aq;
	unsigned long flags;

	lockdep_assert_held(&pipe_av->mutex);

	list_for_each_entry(aq, &ip->queues, node) {
		struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);
		struct ipu_isys_buffer *ib, *ib_safe;

		spin_lock_irqsave(&aq->lock, flags);
		list_for_each_entry_safe(ib, ib_safe, &aq->active, head) {
			struct vb2_buffer *vb =
			    ipu_isys_buffer_to_vb2_buffer(ib);

			list_del(&ib->head);
			if (av->streaming) {
				dev_dbg(&av->isys->adev->dev,
					"%s: queue buffer %u back to incoming\n",
					av->vdev.name,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
					vb->v4l2_buf.index);
#else
					vb->index);
#endif
				/* Queue already streaming, return to driver. */
				list_add(&ib->head, &aq->incoming);
				continue;
			}
			/* Queue not yet streaming, return to user. */
			dev_dbg(&av->isys->adev->dev,
				"%s: return %u back to videobuf2\n",
				av->vdev.name,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
				vb->v4l2_buf.index);
#else
				vb->index);
#endif
			vb2_buffer_done(ipu_isys_buffer_to_vb2_buffer(ib),
					VB2_BUF_STATE_QUEUED);
		}
		spin_unlock_irqrestore(&aq->lock, flags);
	}
}

/*
 * Attempt obtaining a buffer list from the incoming queues, a list of
 * buffers that contains one entry from each video buffer queue. If
 * all queues have no buffers, the buffers that were already dequeued
 * are returned to their queues.
 */
static int buffer_list_get(struct ipu_isys_pipeline *ip,
			   struct ipu_isys_buffer_list *bl)
{
	struct ipu_isys_queue *aq;
	struct ipu_isys_buffer *ib;
	unsigned long flags;
	int ret = 0;

	bl->nbufs = 0;
	INIT_LIST_HEAD(&bl->head);

	list_for_each_entry(aq, &ip->queues, node) {
		struct ipu_isys_buffer *ib;

		spin_lock_irqsave(&aq->lock, flags);
		if (list_empty(&aq->incoming)) {
			spin_unlock_irqrestore(&aq->lock, flags);
			ret = -ENODATA;
			goto error;
		}

		ib = list_last_entry(&aq->incoming,
				     struct ipu_isys_buffer, head);
		if (ib->req) {
			spin_unlock_irqrestore(&aq->lock, flags);
			ret = -ENODATA;
			goto error;
		}

		dev_dbg(&ip->isys->adev->dev, "buffer: %s: buffer %u\n",
			ipu_isys_queue_to_video(aq)->vdev.name,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
			ipu_isys_buffer_to_vb2_buffer(ib)->v4l2_buf.index
#else
			ipu_isys_buffer_to_vb2_buffer(ib)->index
#endif
		    );
		list_del(&ib->head);
		list_add(&ib->head, &bl->head);
		spin_unlock_irqrestore(&aq->lock, flags);

		bl->nbufs++;
	}

	list_for_each_entry(ib, &bl->head, head) {
		struct vb2_buffer *vb = ipu_isys_buffer_to_vb2_buffer(ib);

		aq = vb2_queue_to_ipu_isys_queue(vb->vb2_queue);
		if (aq->prepare_frame_buff_set)
			aq->prepare_frame_buff_set(vb);
	}

	/* Get short packet buffer. */
	if (ip->interlaced && ip->isys->short_packet_source ==
	    IPU_ISYS_SHORT_PACKET_FROM_RECEIVER) {
		ib = ipu_isys_csi2_get_short_packet_buffer(ip, bl);
		if (!ib) {
			ret = -ENODATA;
			dev_err(&ip->isys->adev->dev,
				"No more short packet buffers. Driver bug?");
			WARN_ON(1);
			goto error;
		}
		bl->nbufs++;
	}

	dev_dbg(&ip->isys->adev->dev, "get buffer list %p, %u buffers\n", bl,
		bl->nbufs);
	return ret;

error:
	if (!list_empty(&bl->head))
		ipu_isys_buffer_list_queue(bl,
					   IPU_ISYS_BUFFER_LIST_FL_INCOMING, 0);
	return ret;
}

void
ipu_isys_buffer_to_fw_frame_buff_pin(struct vb2_buffer *vb,
				     struct ipu_fw_isys_frame_buff_set_abi *set)
{
	struct ipu_isys_queue *aq = vb2_queue_to_ipu_isys_queue(vb->vb2_queue);
	struct ipu_isys_video *av = container_of(aq, struct ipu_isys_video, aq);

	if (av->compression)
		set->output_pins[aq->fw_output].compress = 1;

	set->output_pins[aq->fw_output].addr =
	    vb2_dma_contig_plane_dma_addr(vb, 0);
	set->output_pins[aq->fw_output].out_buf_id =
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	    vb->v4l2_buf.index + 1;
#else
	    vb->index + 1;
#endif
}

/*
 * Convert a buffer list to a isys fw ABI framebuffer set. The
 * buffer list is not modified.
 */
#define IPU_ISYS_FRAME_NUM_THRESHOLD  (30)
void
ipu_isys_buffer_to_fw_frame_buff(struct ipu_fw_isys_frame_buff_set_abi *set,
				 struct ipu_isys_pipeline *ip,
				 struct ipu_isys_buffer_list *bl)
{
	struct ipu_isys_buffer *ib;

	WARN_ON(!bl->nbufs);

	set->send_irq_sof = enable_hw_sof_irq ? 0 : 1;
	set->send_resp_sof = enable_hw_sof_irq ? 0 : 1;
	set->send_irq_eof = 0;
	set->send_resp_eof = 0;

	if (ip->streaming)
		set->send_irq_capture_ack = 0;
	else
		set->send_irq_capture_ack = 1;
	set->send_irq_capture_done = 0;

	set->send_resp_capture_ack = 1;
	set->send_resp_capture_done = 1;
	if (!ip->interlaced &&
	    atomic_read(&ip->sequence) >= IPU_ISYS_FRAME_NUM_THRESHOLD) {
		set->send_resp_capture_ack = 0;
		set->send_resp_capture_done = 0;
	}

	list_for_each_entry(ib, &bl->head, head) {
		if (ib->type == IPU_ISYS_VIDEO_BUFFER) {
			struct vb2_buffer *vb =
			    ipu_isys_buffer_to_vb2_buffer(ib);
			struct ipu_isys_queue *aq =
			    vb2_queue_to_ipu_isys_queue(vb->vb2_queue);

			if (aq->fill_frame_buff_set_pin)
				aq->fill_frame_buff_set_pin(vb, set);
		} else if (ib->type == IPU_ISYS_SHORT_PACKET_BUFFER) {
			struct ipu_isys_private_buffer *pb =
			    ipu_isys_buffer_to_private_buffer(ib);
			struct ipu_fw_isys_output_pin_payload_abi *output_pin =
			    &set->output_pins[ip->short_packet_output_pin];

			output_pin->addr = pb->dma_addr;
			output_pin->out_buf_id = pb->index + 1;
		} else {
			WARN_ON(1);
		}
	}
}

/* Start streaming for real. The buffer list must be available. */
static int ipu_isys_stream_start(struct ipu_isys_pipeline *ip,
				 struct ipu_isys_buffer_list *bl, bool error)
{
	struct ipu_isys_video *pipe_av =
	    container_of(ip, struct ipu_isys_video, ip);
	struct ipu_isys_buffer_list __bl;
	int rval;

	mutex_lock(&pipe_av->isys->stream_mutex);

	rval = ipu_isys_video_set_streaming(pipe_av, 1, bl);
	if (rval) {
		mutex_unlock(&pipe_av->isys->stream_mutex);
		goto out_requeue;
	}

	ip->streaming = 1;

	mutex_unlock(&pipe_av->isys->stream_mutex);

	bl = &__bl;

	do {
		struct ipu_fw_isys_frame_buff_set_abi *buf = NULL;
		struct isys_fw_msgs *msg;
		enum ipu_fw_isys_send_type send_type =
		    IPU_FW_ISYS_SEND_TYPE_STREAM_CAPTURE;

		rval = buffer_list_get(ip, bl);
		if (rval == -EINVAL)
			goto out_requeue;
		else if (rval < 0)
			break;

		msg = ipu_get_fw_msg_buf(ip);
		if (!msg)
			return -ENOMEM;

		buf = to_frame_msg_buf(msg);

		ipu_isys_buffer_to_fw_frame_buff(buf, ip, bl);

		ipu_fw_isys_dump_frame_buff_set(&pipe_av->isys->adev->dev, buf,
						ip->nr_output_pins);

		ipu_isys_buffer_list_queue(bl,
					   IPU_ISYS_BUFFER_LIST_FL_ACTIVE, 0);

		rval = ipu_fw_isys_complex_cmd(pipe_av->isys,
					       ip->stream_handle,
					       buf, to_dma_addr(msg),
					       sizeof(*buf),
					       send_type);
	} while (!WARN_ON(rval));

	return 0;

out_requeue:
	if (bl && bl->nbufs)
		ipu_isys_buffer_list_queue(bl,
					   IPU_ISYS_BUFFER_LIST_FL_INCOMING |
					   (error ?
					    IPU_ISYS_BUFFER_LIST_FL_SET_STATE :
					    0),
					   error ? VB2_BUF_STATE_ERROR :
					   VB2_BUF_STATE_QUEUED);
	flush_firmware_streamon_fail(ip);

	return rval;
}

static void buf_queue(struct vb2_buffer *vb)
{
	struct ipu_isys_queue *aq = vb2_queue_to_ipu_isys_queue(vb->vb2_queue);
	struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);
	struct ipu_isys_buffer *ib = vb2_buffer_to_ipu_isys_buffer(vb);
	struct media_pipeline *mp = media_entity_pipeline(&av->vdev.entity);
	struct ipu_isys_pipeline *ip = to_ipu_isys_pipeline(mp);
	struct ipu_isys_buffer_list bl;

	struct ipu_fw_isys_frame_buff_set_abi *buf = NULL;
	struct isys_fw_msgs *msg;

	struct ipu_isys_video *pipe_av =
	    container_of(ip, struct ipu_isys_video, ip);
	unsigned long flags;
	unsigned int i;
	int rval;

	dev_dbg(&av->isys->adev->dev, "buffer: %s: buf_queue %u\n",
		av->vdev.name,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
		vb->v4l2_buf.index
#else
		vb->index
#endif
	    );

	for (i = 0; i < vb->num_planes; i++)
		dev_dbg(&av->isys->adev->dev, "iova: plane %u iova 0x%x\n", i,
			(u32)vb2_dma_contig_plane_dma_addr(vb, i));

	spin_lock_irqsave(&aq->lock, flags);
	list_add(&ib->head, &aq->incoming);
	spin_unlock_irqrestore(&aq->lock, flags);

	mutex_unlock(&av->mutex);
	mutex_lock(&av->isys->reset_mutex);
	while (av->isys->in_reset) {
		mutex_unlock(&av->isys->reset_mutex);
		dev_dbg(&av->isys->adev->dev, "buffer: %s: wait for reset\n",
			av->vdev.name
		);
		usleep_range(10000, 11000);
		mutex_lock(&av->isys->reset_mutex);
	}
	mutex_unlock(&av->isys->reset_mutex);
	mutex_lock(&av->mutex);

	/* ip may be cleared in ipu reset */
	ip = to_ipu_isys_pipeline(media_entity_pipeline(&av->vdev.entity));
	pipe_av = container_of(ip, struct ipu_isys_video, ip);
	if (ib->req)
		return;

	if (!pipe_av || !mp || !vb->vb2_queue->start_streaming_called) {
		dev_dbg(&av->isys->adev->dev,
			"no pipe or streaming, adding to incoming\n");
		return;
	}

	mutex_unlock(&av->mutex);
	mutex_lock(&pipe_av->mutex);

	if (ip->nr_streaming != ip->nr_queues) {
		dev_dbg(&av->isys->adev->dev,
			"not streaming yet, adding to incoming\n");
		goto out;
	}

	/*
	 * We just put one buffer to the incoming list of this queue
	 * (above). Let's see whether all queues in the pipeline would
	 * have a buffer.
	 */
	rval = buffer_list_get(ip, &bl);
	if (rval < 0) {
		if (rval == -EINVAL) {
			dev_err(&av->isys->adev->dev,
				"error: buffer list get failed\n");
			WARN_ON(1);
		} else {
			dev_dbg(&av->isys->adev->dev,
				"not enough buffers available\n");
		}
		goto out;
	}

	msg = ipu_get_fw_msg_buf(ip);
	if (!msg) {
		rval = -ENOMEM;
		goto out;
	}
	buf = to_frame_msg_buf(msg);

	ipu_isys_buffer_to_fw_frame_buff(buf, ip, &bl);

	ipu_fw_isys_dump_frame_buff_set(&pipe_av->isys->adev->dev, buf,
					ip->nr_output_pins);

	if (!ip->streaming) {
		dev_dbg(&av->isys->adev->dev,
			"got a buffer to start streaming!\n");
		rval = ipu_isys_stream_start(ip, &bl, true);
		if (rval)
			dev_err(&av->isys->adev->dev,
				"stream start failed.\n");
		goto out;
	}

	/*
	 * We must queue the buffers in the buffer list to the
	 * appropriate video buffer queues BEFORE passing them to the
	 * firmware since we could get a buffer event back before we
	 * have queued them ourselves to the active queue.
	 */
	ipu_isys_buffer_list_queue(&bl, IPU_ISYS_BUFFER_LIST_FL_ACTIVE, 0);

	rval = ipu_fw_isys_complex_cmd(pipe_av->isys,
				       ip->stream_handle,
				       buf, to_dma_addr(msg),
				       sizeof(*buf),
				       IPU_FW_ISYS_SEND_TYPE_STREAM_CAPTURE);
	if (!WARN_ON(rval < 0))
		dev_dbg(&av->isys->adev->dev, "queued buffer\n");

out:
	mutex_unlock(&pipe_av->mutex);
	mutex_lock(&av->mutex);
}

int ipu_isys_link_fmt_validate(struct ipu_isys_queue *aq)
{
	struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);
	struct v4l2_subdev_format fmt = { 0 };
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
	struct media_pad *pad = media_entity_remote_pad(av->vdev.entity.pads);
#else
	struct media_pad *pad =
		media_pad_remote_pad_first(av->vdev.entity.pads);
#endif
	struct v4l2_subdev *sd;
	int rval;

	if (!pad) {
		dev_dbg(&av->isys->adev->dev,
			"video node %s pad not connected\n", av->vdev.name);
		return -ENOTCONN;
	}

	sd = media_entity_to_v4l2_subdev(pad->entity);

	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.pad = pad->index;
	rval = v4l2_subdev_call(sd, pad, get_fmt, NULL, &fmt);
	if (rval)
		return rval;

	if (fmt.format.width != av->mpix.width ||
	    fmt.format.height != av->mpix.height) {
		dev_dbg(&av->isys->adev->dev,
			"wrong width or height %ux%u (%ux%u expected)\n",
			av->mpix.width, av->mpix.height,
			fmt.format.width, fmt.format.height);
		return -EINVAL;
	}

	if (fmt.format.field != av->mpix.field) {
		dev_dbg(&av->isys->adev->dev,
			"wrong field value 0x%8.8x (0x%8.8x expected)\n",
			av->mpix.field, fmt.format.field);
		return -EINVAL;
	}

	if (fmt.format.code != av->pfmt->code) {
		dev_dbg(&av->isys->adev->dev,
			"wrong media bus code 0x%8.8x (0x%8.8x expected)\n",
			av->pfmt->code, fmt.format.code);
		return -EINVAL;
	}

	return 0;
}

/* Return buffers back to videobuf2. */
static void return_buffers(struct ipu_isys_queue *aq,
			   enum vb2_buffer_state state)
{
	struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);
	int reset_needed = 0;
	unsigned long flags;

	spin_lock_irqsave(&aq->lock, flags);
	while (!list_empty(&aq->incoming)) {
		struct ipu_isys_buffer *ib = list_first_entry(&aq->incoming,
							      struct
							      ipu_isys_buffer,
							      head);
		struct vb2_buffer *vb = ipu_isys_buffer_to_vb2_buffer(ib);

		list_del(&ib->head);
		spin_unlock_irqrestore(&aq->lock, flags);

		vb2_buffer_done(vb, state);

		dev_dbg(&av->isys->adev->dev,
			"%s: stop_streaming incoming %u\n",
			ipu_isys_queue_to_video(vb2_queue_to_ipu_isys_queue
						(vb->vb2_queue))->vdev.name,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
			vb->v4l2_buf.index);
#else
			vb->index);
#endif

		spin_lock_irqsave(&aq->lock, flags);
	}

	/*
	 * Something went wrong (FW crash / HW hang / not all buffers
	 * returned from isys) if there are still buffers queued in active
	 * queue. We have to clean up places a bit.
	 */
	while (!list_empty(&aq->active)) {
		struct ipu_isys_buffer *ib = list_first_entry(&aq->active,
							      struct
							      ipu_isys_buffer,
							      head);
		struct vb2_buffer *vb = ipu_isys_buffer_to_vb2_buffer(ib);

		list_del(&ib->head);
		spin_unlock_irqrestore(&aq->lock, flags);

		vb2_buffer_done(vb, state);

		dev_warn(&av->isys->adev->dev, "%s: cleaning active queue %u\n",
			 ipu_isys_queue_to_video(vb2_queue_to_ipu_isys_queue
						 (vb->vb2_queue))->vdev.name,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
			 vb->v4l2_buf.index);
#else
			 vb->index);
#endif

		spin_lock_irqsave(&aq->lock, flags);
		reset_needed = 1;
	}

	spin_unlock_irqrestore(&aq->lock, flags);

	if (reset_needed) {
		mutex_lock(&av->isys->mutex);
		av->isys->reset_needed = true;
		mutex_unlock(&av->isys->mutex);
	}
}

static int __start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct ipu_isys_queue *aq = vb2_queue_to_ipu_isys_queue(q);
	struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);
	struct ipu_isys_video *pipe_av;
	struct ipu_isys_pipeline *ip;
	struct ipu_isys_buffer_list __bl, *bl = NULL;
	bool first;
	int rval;

	dev_dbg(&av->isys->adev->dev,
		"stream: %s: width %u, height %u, css pixelformat %u\n",
		av->vdev.name, av->mpix.width, av->mpix.height,
		av->pfmt->css_pixelformat);

	mutex_lock(&av->isys->stream_mutex);

	first = !media_entity_pipeline(&av->vdev.entity);

	if (first) {
		rval = ipu_isys_video_prepare_streaming(av, 1);
		if (rval) {
			dev_err(&av->isys->adev->dev,
				"%s: prepare stream: failed (%d)\n",
				av->vdev.name, rval);
			goto out_return_buffers;
		}
	}

	mutex_unlock(&av->isys->stream_mutex);

	ip = to_ipu_isys_pipeline(media_entity_pipeline(&av->vdev.entity));
	pipe_av = container_of(ip, struct ipu_isys_video, ip);
	if (pipe_av != av) {
		mutex_unlock(&av->mutex);
		mutex_lock(&pipe_av->mutex);
	}

	ip->nr_streaming++;
	dev_dbg(&av->isys->adev->dev, "queue %u of %u\n", ip->nr_streaming,
		ip->nr_queues);
	list_add(&aq->node, &ip->queues);
	if (ip->nr_streaming != ip->nr_queues) {
		dev_dbg(&av->isys->adev->dev,
			"%s: streaming queue not match (%d)(%d)\n",
			av->vdev.name, ip->nr_streaming, ip->nr_queues);
		goto out;
	}

	if (list_empty(&av->isys->requests)) {
		bl = &__bl;
		rval = buffer_list_get(ip, bl);
		if (rval == -EINVAL) {
			dev_err(&av->isys->adev->dev,
				"buffer list invalid\n");
			goto out_stream_start;
		} else if (rval < 0) {
			dev_dbg(&av->isys->adev->dev,
				"no request available, postponing streamon\n");
			goto out;
		}
	}

	rval = ipu_isys_stream_start(ip, bl, false);
	if (rval) {
		dev_err(&av->isys->adev->dev,
			"isys stream start failed\n");
		goto out_stream_start;
	}

out:
	if (pipe_av != av) {
		mutex_unlock(&pipe_av->mutex);
		mutex_lock(&av->mutex);
	}

	return 0;

out_stream_start:
	list_del(&aq->node);
	ip->nr_streaming--;
	if (pipe_av != av) {
		mutex_unlock(&pipe_av->mutex);
		mutex_lock(&av->mutex);
	}

	mutex_lock(&av->isys->stream_mutex);
	if (first)
		ipu_isys_video_prepare_streaming(av, 0);

out_return_buffers:
	mutex_unlock(&av->isys->stream_mutex);
	return_buffers(aq, VB2_BUF_STATE_QUEUED);

	return rval;
}

static int start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct ipu_isys_queue *aq = vb2_queue_to_ipu_isys_queue(q);
	struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);
	int rval;

	mutex_unlock(&av->mutex);
	mutex_lock(&av->isys->reset_mutex);
	while (av->isys->in_stop_streaming) {
		mutex_unlock(&av->isys->reset_mutex);
		dev_dbg(&av->isys->adev->dev, "buffer: %s: wait for stop streaming\n",
			av->vdev.name
		);
		usleep_range(10000, 11000);
		mutex_lock(&av->isys->reset_mutex);
	}
	mutex_unlock(&av->isys->reset_mutex);
	mutex_lock(&av->mutex);

	rval = __start_streaming(q, count);
	if (rval)
		av->start_streaming = 0;
	else
		av->start_streaming = 1;

	return rval;
}

static void reset_stop_streaming(struct ipu_isys_video *av)
{
	struct ipu_isys_pipeline *ip =
		to_ipu_isys_pipeline(media_entity_pipeline(&av->vdev.entity));
	struct ipu_isys_queue *aq = &av->aq;

	dev_dbg(&av->isys->adev->dev, "%s: stop streaming\n", av->vdev.name);

	mutex_lock(&av->isys->stream_mutex);
	if (ip->nr_streaming == ip->nr_queues && ip->streaming)
		ipu_isys_video_set_streaming(av, 0, NULL);
	if (ip->nr_streaming == 1)
		ipu_isys_video_prepare_streaming(av, 0);
	else
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
		av->vdev.entity.pipe = NULL;
#else
		av->vdev.entity.pads[0].pipe = NULL;
#endif
	mutex_unlock(&av->isys->stream_mutex);

	ip->nr_streaming--;
	list_del(&aq->node);
	ip->streaming = 0;
	av->start_streaming = 0;
}

static int reset_start_streaming(struct ipu_isys_video *av)
{
	struct ipu_isys_queue *aq = &av->aq;
	unsigned long flags;
	int rval;

	dev_dbg(&av->isys->adev->dev, "%s: start streaming\n", av->vdev.name);

	spin_lock_irqsave(&aq->lock, flags);
	while (!list_empty(&aq->active)) {
		struct ipu_isys_buffer *ib = list_first_entry(&aq->active,
				struct
				ipu_isys_buffer,
				head);

		list_del(&ib->head);
		list_add_tail(&ib->head, &aq->incoming);
	}
	spin_unlock_irqrestore(&aq->lock, flags);

	av->skipframe = 1;
	rval = __start_streaming(&aq->vbq, 0);
	if (rval) {
		dev_dbg(&av->isys->adev->dev,
			"%s: start streaming failed in reset ! set av->start_streaming = 0.\n",
			av->vdev.name);
		av->start_streaming = 0;
	} else
		av->start_streaming = 1;

	return rval;
}

static int ipu_isys_reset(struct ipu_isys_video *self_av,
			  struct ipu_isys_pipeline *self_ip)
{
	struct ipu_isys *isys = self_av->isys;
	struct ipu_bus_device *adev = isys->adev;
	struct ipu_device *isp = isys->adev->isp;
	struct ipu_isys_video *av = NULL;
	struct ipu_isys_pipeline *ip = NULL;
	struct ipu_isys_csi2_be_soc *csi2_be_soc = NULL;
	int rval, i, j;
	int has_streaming = 0;

	dev_dbg(&isys->adev->dev, "%s\n", __func__);

	mutex_lock(&isys->reset_mutex);
	if (isys->in_reset) {
		mutex_unlock(&isys->reset_mutex);
		return 0;
	}
	isys->in_reset = true;

	while (isys->in_stop_streaming) {
		dev_dbg(&isys->adev->dev, "isys reset: %s: wait for stop\n",
			self_av->vdev.name);
		mutex_unlock(&isys->reset_mutex);
		usleep_range(10000, 11000);
		mutex_lock(&isys->reset_mutex);
	}

	mutex_unlock(&isys->reset_mutex);

	av = &isys->csi2->av;
	ip = to_ipu_isys_pipeline(media_entity_pipeline(&av->vdev.entity));

	if (av != self_av && ip && ip != self_ip) {
		mutex_lock(&av->mutex);
		if (ip->streaming && !ip->nr_streaming) {
			av->reset = true;
			has_streaming = true;
			reset_stop_streaming(av);
		}
		mutex_unlock(&av->mutex);
	}

	av = &isys->csi2_be.av;
	ip = to_ipu_isys_pipeline(media_entity_pipeline(&av->vdev.entity));

	if (av != self_av && ip && ip != self_ip) {
		mutex_lock(&av->mutex);
		if (ip->streaming && !ip->nr_streaming) {
			av->reset = true;
			has_streaming = true;
			reset_stop_streaming(av);
		}
		mutex_unlock(&av->mutex);
	}

	for (i = 0; i < NR_OF_CSI2_BE_SOC_DEV; i++) {
		csi2_be_soc = &isys->csi2_be_soc[i];
		for (j = 0; j < vnode_num; j++) {
			av = &csi2_be_soc->av[j];
		if (av == self_av)
			continue;

		ip = to_ipu_isys_pipeline
			(media_entity_pipeline(&av->vdev.entity));
		if (!ip || ip == self_ip)
			continue;

		mutex_lock(&av->mutex);
		if (!ip->streaming && !ip->nr_streaming) {
			mutex_unlock(&av->mutex);
			continue;
		}

		if (!av->start_streaming) {
			mutex_unlock(&av->mutex);
			continue;
		}

		av->reset = true;
		has_streaming = true;
		reset_stop_streaming(av);
		mutex_unlock(&av->mutex);
		}
	}

	if (!has_streaming)
		goto end_of_reset;

	dev_dbg(&isys->adev->dev, "ipu reset, close fw\n");
	ipu_fw_isys_close(isys);

	dev_dbg(&isys->adev->dev, "ipu reset, power cycle\n");

	/* bus_pm_runtime_suspend() */
	/* isys_runtime_pm_suspend() */
	adev->dev.bus->pm->runtime_suspend(&adev->dev);

	/* ipu_suspend */
	isp->pdev->driver->driver.pm->runtime_suspend(&isp->pdev->dev);

	/* ipu_runtime_resume */
	isp->pdev->driver->driver.pm->runtime_resume(&isp->pdev->dev);

	/* bus_pm_runtime_resume() */
	/* isys_runtime_pm_resume() */
	adev->dev.bus->pm->runtime_resume(&adev->dev);

	ipu_configure_spc(isys->adev->isp,
			  &isys->pdata->ipdata->hw_variant,
			  IPU_CPD_PKG_DIR_ISYS_SERVER_IDX,
			  isys->pdata->base, isys->pkg_dir,
			  isys->pkg_dir_dma_addr);

	ipu_cleanup_fw_msg_bufs(isys);
	if (isys->fwcom) {
		dev_err(&isys->adev->dev, "Clearing old context\n");
		ipu_fw_isys_cleanup(isys);
	}

	rval = ipu_fw_isys_init(av->isys,
			  isys->pdata->ipdata->num_parallel_streams);
	if (rval < 0)
		dev_err(&isys->adev->dev, "ipu fw isys init failed\n");

	dev_dbg(&isys->adev->dev, "restart streams\n");

	av = &isys->csi2->av;
	if (av->reset) {
		av->reset = false;
		mutex_lock(&av->mutex);
		reset_start_streaming(av);
		mutex_unlock(&av->mutex);
	}

	av = &isys->csi2_be.av;
	if (av->reset) {
		av->reset = false;
		mutex_lock(&av->mutex);
		reset_start_streaming(av);
		mutex_unlock(&av->mutex);
	}

	for (i = 0; i < NR_OF_CSI2_BE_SOC_DEV; i++) {
		csi2_be_soc = &isys->csi2_be_soc[i];
		for (j = 0; j < vnode_num; j++) {
			av = &csi2_be_soc->av[j];
		if (!av->reset)
			continue;

		av->reset = false;
		mutex_lock(&av->mutex);
		reset_start_streaming(av);
		mutex_unlock(&av->mutex);
		}
	}

end_of_reset:
	mutex_lock(&isys->reset_mutex);
	isys->in_reset = false;
	mutex_unlock(&isys->reset_mutex);
	dev_dbg(&isys->adev->dev, "reset done\n");

	return 0;
}

static void stop_streaming(struct vb2_queue *q)
{
	struct ipu_isys_queue *aq = vb2_queue_to_ipu_isys_queue(q);
	struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);
	struct media_pipeline *mp = media_entity_pipeline(&av->vdev.entity);
	struct ipu_isys_pipeline *ip = to_ipu_isys_pipeline(mp);
	struct ipu_isys_video *pipe_av =
		container_of(ip, struct ipu_isys_video, ip);
	dev_dbg(&av->isys->adev->dev, "stop: %s: enter\n",
		av->vdev.name);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
	struct media_pad *source_pad = media_entity_remote_pad(&av->pad);
#else
	struct media_pad *source_pad = media_pad_remote_pad_first(&av->pad);
#endif

	if (!source_pad) {
		dev_err(&av->isys->adev->dev, "stop stream: no link.\n");
		return;
	}

	mutex_unlock(&av->mutex);
	mutex_lock(&av->isys->reset_mutex);
	while (av->isys->in_reset || av->isys->in_stop_streaming) {
		mutex_unlock(&av->isys->reset_mutex);
		dev_dbg(&av->isys->adev->dev, "stop: %s: wait for in_reset = %d\n",
			av->vdev.name, av->isys->in_reset);
		dev_dbg(&av->isys->adev->dev, "stop: %s: wait for in_stop = %d\n",
			av->vdev.name, av->isys->in_stop_streaming);
		usleep_range(10000, 11000);
		mutex_lock(&av->isys->reset_mutex);
	}

	if (!av->start_streaming) {
		mutex_unlock(&av->isys->reset_mutex);
		return;
	}

	av->isys->in_stop_streaming = true;
	mutex_unlock(&av->isys->reset_mutex);

	ip = to_ipu_isys_pipeline(media_entity_pipeline(&av->vdev.entity));
	pipe_av = container_of(ip, struct ipu_isys_video, ip);

	mutex_lock(&av->mutex);

	if (!ip) {
		dev_err(&av->isys->adev->dev, "stop: %s: ip cleard!\n",
			av->vdev.name);
		return_buffers(aq, VB2_BUF_STATE_ERROR);
		mutex_lock(&av->isys->reset_mutex);
		av->isys->in_stop_streaming = false;
		mutex_unlock(&av->isys->reset_mutex);
		return;
	}

	if (pipe_av != av) {
		mutex_unlock(&av->mutex);
		mutex_lock(&pipe_av->mutex);
	}

	mutex_lock(&av->isys->stream_mutex);
	if (ip->nr_streaming == ip->nr_queues && ip->streaming)
		ipu_isys_video_set_streaming(av, 0, NULL);
	if (ip->nr_streaming == 1)
		ipu_isys_video_prepare_streaming(av, 0);
	else
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
		av->vdev.entity.pipe = NULL;
#else
		av->vdev.entity.pads[0].pipe = NULL;
#endif
	mutex_unlock(&av->isys->stream_mutex);

	ip->nr_streaming--;
	list_del(&aq->node);
	ip->streaming = 0;

	if (pipe_av != av) {
		mutex_unlock(&pipe_av->mutex);
		mutex_lock(&av->mutex);
	}

	return_buffers(aq, VB2_BUF_STATE_ERROR);
	av->start_streaming = 0;
	mutex_lock(&av->isys->reset_mutex);
	av->isys->in_stop_streaming = false;
	mutex_unlock(&av->isys->reset_mutex);

	if (av->isys->reset_needed) {
		if (!ip->nr_streaming && (!is_support_vc(source_pad, ip) || is_has_metadata(ip)))
			ipu_isys_reset(av, ip);
		else
			av->isys->reset_needed = 0;
	}

	dev_dbg(&av->isys->adev->dev, "stop: %s: exit\n",
		av->vdev.name);
}

static unsigned int
get_sof_sequence_by_timestamp(struct ipu_isys_pipeline *ip,
			      struct ipu_fw_isys_resp_info_abi *info)
{
	struct ipu_isys *isys =
	    container_of(ip, struct ipu_isys_video, ip)->isys;
	u64 time = (u64)info->timestamp[1] << 32 | info->timestamp[0];
	unsigned int i;

	/*
	 * The timestamp is invalid as no TSC in some FPGA platform,
	 * so get the sequence from pipeline directly in this case.
	 */
	if (time == 0)
		return atomic_read(&ip->sequence) - 1;

	for (i = 0; i < IPU_ISYS_MAX_PARALLEL_SOF; i++)
		if (time == ip->seq[i].timestamp) {
			dev_dbg(&isys->adev->dev,
				"sof: using seq nr %u for ts 0x%16.16llx\n",
				ip->seq[i].sequence, time);
			return ip->seq[i].sequence;
		}

	dev_dbg(&isys->adev->dev, "SOF: looking for 0x%16.16llx\n", time);
	for (i = 0; i < IPU_ISYS_MAX_PARALLEL_SOF; i++)
		dev_dbg(&isys->adev->dev,
			"SOF: sequence %u, timestamp value 0x%16.16llx\n",
			ip->seq[i].sequence, ip->seq[i].timestamp);
	dev_dbg(&isys->adev->dev, "SOF sequence number not found\n");

	return 0;
}

static u64 get_sof_ns_delta(struct ipu_isys_video *av,
			    struct ipu_fw_isys_resp_info_abi *info)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(&av->isys->adev->dev);
	struct ipu_device *isp = adev->isp;
	u64 delta, tsc_now;

	if (!ipu_buttress_tsc_read(isp, &tsc_now))
		delta = tsc_now -
		    ((u64)info->timestamp[1] << 32 | info->timestamp[0]);
	else
		delta = 0;

	return ipu_buttress_tsc_ticks_to_ns(delta, isp);
}

void
ipu_isys_buf_calc_sequence_time(struct ipu_isys_buffer *ib,
				struct ipu_fw_isys_resp_info_abi *info)
{
	struct vb2_buffer *vb = ipu_isys_buffer_to_vb2_buffer(ib);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	struct timespec ts_now;
#endif
	struct ipu_isys_queue *aq = vb2_queue_to_ipu_isys_queue(vb->vb2_queue);
	struct ipu_isys_video *av = ipu_isys_queue_to_video(aq);
	struct device *dev = &av->isys->adev->dev;
	struct media_pipeline *mp = media_entity_pipeline(&av->vdev.entity);
	struct ipu_isys_pipeline *ip = to_ipu_isys_pipeline(mp);
	u64 ns;
	u32 sequence;

	if (ip->has_sof) {
		ns = (wall_clock_ts_on) ? ktime_get_real_ns() : ktime_get_ns();
		ns -= get_sof_ns_delta(av, info);
		sequence = get_sof_sequence_by_timestamp(ip, info);
	} else {
		ns = ((wall_clock_ts_on) ? ktime_get_real_ns() :
		      ktime_get_ns());
		sequence = (atomic_inc_return(&ip->sequence) - 1)
		    / ip->nr_queues;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	vb->v4l2_buf.sequence = sequence;
	ts_now = ns_to_timespec(ns);
	vb->v4l2_buf.timestamp.tv_sec = ts_now.tv_sec;
	vb->v4l2_buf.timestamp.tv_usec = ts_now.tv_nsec / NSEC_PER_USEC;

	dev_dbg(dev, "buffer: %s: buffer done %u\n", av->vdev.name,
		vb->v4l2_buf.index);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	vbuf->sequence = sequence;
	ts_now = ns_to_timespec(ns);
	vbuf->timestamp.tv_sec = ts_now.tv_sec;
	vbuf->timestamp.tv_usec = ts_now.tv_nsec / NSEC_PER_USEC;

	dev_dbg(dev, "%s: buffer done %u\n", av->vdev.name,
		vb->index);
#else
	vbuf->vb2_buf.timestamp = ns;
	vbuf->sequence = sequence;

	dev_dbg(dev, "buf: %s: buffer done, CPU-timestamp:%lld, sequence:%d\n",
		av->vdev.name, ktime_get_ns(), sequence);
	dev_dbg(dev, "index:%d, vbuf timestamp:%lld, endl\n",
		vb->index, vbuf->vb2_buf.timestamp);
#endif
}

void ipu_isys_queue_buf_done(struct ipu_isys_buffer *ib)
{
	struct vb2_buffer *vb = ipu_isys_buffer_to_vb2_buffer(ib);

	if (atomic_read(&ib->ib_err_flag)) {
		vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
		/*
		 * Operation on buffer is ended with error and will be reported
		 * to the userspace when it is de-queued
		 */
		atomic_set(&ib->ib_err_flag, 0);
	} else if (atomic_read(&ib->skipframe_flag)) {
		vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
		atomic_set(&ib->skipframe_flag, 0);
	} else {
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
	}
}

void ipu_isys_queue_buf_ready(struct ipu_isys_pipeline *ip,
			      struct ipu_fw_isys_resp_info_abi *info)
{
	struct ipu_isys *isys =
	    container_of(ip, struct ipu_isys_video, ip)->isys;
	struct ipu_isys_queue *aq = ip->output_pins[info->pin_id].aq;
	struct ipu_isys_buffer *ib;
	struct vb2_buffer *vb;
	unsigned long flags;
	bool first = true;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	struct v4l2_buffer *buf;
#else
	struct vb2_v4l2_buffer *buf;
#endif

	dev_dbg(&isys->adev->dev, "buffer: %s: received buffer %8.8x\n",
		ipu_isys_queue_to_video(aq)->vdev.name, info->pin.addr);

	spin_lock_irqsave(&aq->lock, flags);
	if (list_empty(&aq->active)) {
		spin_unlock_irqrestore(&aq->lock, flags);
		dev_err(&isys->adev->dev, "active queue empty\n");
		return;
	}

	list_for_each_entry_reverse(ib, &aq->active, head) {
		dma_addr_t addr;

		vb = ipu_isys_buffer_to_vb2_buffer(ib);
		addr = vb2_dma_contig_plane_dma_addr(vb, 0);

		if (info->pin.addr != addr) {
			if (first)
				dev_err(&isys->adev->dev,
					"WARN: buffer address %pad expected!\n",
					&addr);
			first = false;
			continue;
		}
		u32 mask = 0;
		u32 csi2_status = ip->csi2->receiver_errors;
		u32 irq = readl(ip->csi2->base + CSI_PORT_REG_BASE_IRQ_CSI +
			CSI_PORT_REG_BASE_IRQ_STATUS_OFFSET);

		mask = (ipu_ver == IPU_VER_6 || ipu_ver == IPU_VER_6EP ||
			ipu_ver == IPU_VER_6EP_MTL) ?
			IPU6_CSI_RX_ERROR_IRQ_MASK : IPU6SE_CSI_RX_ERROR_IRQ_MASK;

		csi2_status |= irq & mask;

		if (info->error_info.error ==
		    IPU_FW_ISYS_ERROR_HW_REPORTED_STR2MMIO ||
		    csi2_status != 0) {
			/*
			 * Check for error message:
			 * 'IPU_FW_ISYS_ERROR_HW_REPORTED_STR2MMIO' &
			 * CSI2 errors
			 */
			dev_dbg(&isys->adev->dev, "buffer: csi2_status: 0x%x, fw error: %d\n",
				csi2_status, info->error_info.error);
			atomic_set(&ib->ib_err_flag, 1);
		}
		dev_dbg(&isys->adev->dev, "buffer: found buffer %pad\n", &addr);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
		buf = &vb->v4l2_buf;
#else
		buf = to_vb2_v4l2_buffer(vb);
#endif
		buf->field = V4L2_FIELD_NONE;

		list_del(&ib->head);
		spin_unlock_irqrestore(&aq->lock, flags);

		ipu_isys_buf_calc_sequence_time(ib, info);

		struct vb2_buffer *vb = ipu_isys_buffer_to_vb2_buffer(ib);
		struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

		if (atomic_read(&ib->ib_err_flag))
			dev_err(&isys->adev->dev, "csi2-%i error: #%d\n",
					ip->csi2->index, vbuf->sequence);
		/*
		 * For interlaced buffers, the notification to user space
		 * is postponed to capture_done event since the field
		 * information is available only at that time.
		 */
		if (ip->interlaced) {
			spin_lock_irqsave(&ip->short_packet_queue_lock, flags);
			list_add(&ib->head, &ip->pending_interlaced_bufs);
			spin_unlock_irqrestore(&ip->short_packet_queue_lock,
					       flags);
		} else {
			ipu_isys_queue_buf_done(ib);
		}

		return;
	}

	dev_err(&isys->adev->dev,
		"WARNING: cannot find a matching video buffer!\n");

	spin_unlock_irqrestore(&aq->lock, flags);
}

void
ipu_isys_queue_short_packet_ready(struct ipu_isys_pipeline *ip,
				  struct ipu_fw_isys_resp_info_abi *info)
{
	struct ipu_isys *isys =
	    container_of(ip, struct ipu_isys_video, ip)->isys;
	unsigned long flags;

	dev_dbg(&isys->adev->dev, "receive short packet buffer %8.8x\n",
		info->pin.addr);
	spin_lock_irqsave(&ip->short_packet_queue_lock, flags);
	ip->cur_field = ipu_isys_csi2_get_current_field(ip, info->timestamp);
	spin_unlock_irqrestore(&ip->short_packet_queue_lock, flags);
}

struct vb2_ops ipu_isys_queue_ops = {
	.queue_setup = queue_setup,
	.wait_prepare = ipu_isys_queue_unlock,
	.wait_finish = ipu_isys_queue_lock,
	.buf_init = buf_init,
	.buf_prepare = buf_prepare,
	.buf_finish = buf_finish,
	.buf_cleanup = buf_cleanup,
	.start_streaming = start_streaming,
	.stop_streaming = stop_streaming,
	.buf_queue = buf_queue,
};

int ipu_isys_queue_init(struct ipu_isys_queue *aq)
{
	struct ipu_isys *isys = ipu_isys_queue_to_video(aq)->isys;
	int rval;

	if (!aq->vbq.io_modes)
		aq->vbq.io_modes = VB2_USERPTR | VB2_MMAP | VB2_DMABUF;
	aq->vbq.drv_priv = aq;
	aq->vbq.ops = &ipu_isys_queue_ops;
	aq->vbq.mem_ops = &vb2_dma_contig_memops;
	aq->vbq.timestamp_flags = (wall_clock_ts_on) ?
	    V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN : V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;

	rval = vb2_queue_init(&aq->vbq);
	if (rval)
		return rval;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	aq->ctx = vb2_dma_contig_init_ctx(&isys->adev->dev);
	if (IS_ERR(aq->ctx)) {
		vb2_queue_release(&aq->vbq);
		return PTR_ERR(aq->ctx);
	}
#else
	aq->dev = &isys->adev->dev;
	aq->vbq.dev = &isys->adev->dev;
#endif
	spin_lock_init(&aq->lock);
	INIT_LIST_HEAD(&aq->active);
	INIT_LIST_HEAD(&aq->incoming);

	return 0;
}

void ipu_isys_queue_cleanup(struct ipu_isys_queue *aq)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	if (IS_ERR_OR_NULL(aq->ctx))
		return;

	vb2_dma_contig_cleanup_ctx(aq->ctx);
	aq->ctx = NULL;
#endif
	vb2_queue_release(&aq->vbq);
}
