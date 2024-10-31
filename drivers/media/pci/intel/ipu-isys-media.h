/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2016 - 2024 Intel Corporation */

#ifndef IPU_ISYS_MEDIA_H
#define IPU_ISYS_MEDIA_H

#include <linux/slab.h>
#include <media/media-entity.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
#define is_media_entity_v4l2_subdev(e) \
	(media_entity_type(e) == MEDIA_ENT_T_V4L2_SUBDEV)
#define is_media_entity_v4l2_io(e) \
	(media_entity_type(e) == MEDIA_ENT_T_DEVNODE)
#define media_create_pad_link(a, b, c, d, e)	\
	media_entity_create_link(a, b, c, d, e)
#define media_entity_pads_init(a, b, c)	\
	media_entity_init(a, b, c, 0)
#define media_entity_id(ent) ((ent)->id)
#define media_entity_graph_walk_init(a, b) 0
#define media_entity_graph_walk_cleanup(a) do { } while (0)

#define IPU_COMPAT_MAX_ENTITIES MEDIA_ENTITY_ENUM_MAX_ID

struct media_entity_enum {
	unsigned long *bmap;
	int idx_max;
};

static inline int media_entity_enum_init(struct media_entity_enum *ent_enum,
			   struct media_device *mdev)
{
	int idx_max = IPU_COMPAT_MAX_ENTITIES;

	ent_enum->bmap = kcalloc(DIV_ROUND_UP(idx_max, BITS_PER_LONG),
				 sizeof(long), GFP_KERNEL);
	if (!ent_enum->bmap)
		return -ENOMEM;

	bitmap_zero(ent_enum->bmap, idx_max);

	ent_enum->idx_max = idx_max;
	return 0;
}

static inline void media_entity_enum_cleanup(struct media_entity_enum *ent_enum)
{
	kfree(ent_enum->bmap);
}

static inline void media_entity_enum_set(struct media_entity_enum *ent_enum,
					 struct media_entity *entity)
{
	if (media_entity_id(entity) >= ent_enum->idx_max) {
		WARN_ON(1);
		return;
	}
	__set_bit(media_entity_id(entity), ent_enum->bmap);
}

static inline void media_entity_enum_zero(struct media_entity_enum *ent_enum)
{
	bitmap_zero(ent_enum->bmap, ent_enum->idx_max);
}

static inline bool media_entity_enum_test(struct media_entity_enum *ent_enum,
					  struct media_entity *entity)
{
	if (media_entity_id(entity) >= ent_enum->idx_max) {
		WARN_ON(1);
		return false;
	}

	return test_bit(media_entity_id(entity), ent_enum->bmap);
}
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
#define media_pipeline_start(e, p) media_entity_pipeline_start(e, p)

#define media_pipeline_stop(e) media_entity_pipeline_stop(e)

#define media_graph_walk_init(g, d) media_entity_graph_walk_init(g, d)

#define media_graph_walk_start(g, p) media_entity_graph_walk_start(g, p)

#define media_graph_walk_next(g) media_entity_graph_walk_next(g)

#define media_graph_walk_cleanup(g) media_entity_graph_walk_cleanup(g)
#endif

struct __packed media_request_cmd {
	__u32 cmd;
	__u32 request;
	__u32 flags;
};

struct __packed media_event_request_complete {
	__u32 id;
};

#define MEDIA_EVENT_TYPE_REQUEST_COMPLETE	1

struct __packed media_event {
	__u32 type;
	__u32 sequence;
	__u32 reserved[4];

	union {
		struct media_event_request_complete req_complete;
	};
};

enum media_device_request_state {
	MEDIA_DEVICE_REQUEST_STATE_IDLE,
	MEDIA_DEVICE_REQUEST_STATE_QUEUED,
	MEDIA_DEVICE_REQUEST_STATE_DELETED,
	MEDIA_DEVICE_REQUEST_STATE_COMPLETE,
};

struct media_kevent {
	struct list_head list;
	struct media_event ev;
};

struct media_device_request {
	u32 id;
	struct media_device *mdev;
	struct file *filp;
	struct media_kevent *kev;
	struct kref kref;
	struct list_head list;
	struct list_head fh_list;
	enum media_device_request_state state;
	struct list_head data;
	u32 flags;
};

static inline struct media_device_request *
media_device_request_find(struct media_device *mdev, u16 reqid)
{
	return NULL;
}

static inline void media_device_request_get(struct media_device_request *req)
{
}

static inline void media_device_request_put(struct media_device_request *req)
{
}

static inline void
media_device_request_complete(struct media_device *mdev,
			      struct media_device_request *req)
{
}

#endif /* IPU_ISYS_MEDIA_H */
