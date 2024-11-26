/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "video_async.h"
#include "video_device.h"

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/slist.h>

LOG_MODULE_REGISTER(video_async, CONFIG_VIDEO_LOG_LEVEL);

static sys_slist_t notifiers_list;

/* Find a registered notifier associated with this device */
struct video_notifier *video_async_find_notifier(const struct device *dev)
{
	struct video_notifier *nf;

	SYS_SLIST_FOR_EACH_CONTAINER(&notifiers_list, nf, node) {
		if (nf->dev == dev) {
			return nf;
		}
	}

	return NULL;
}

/* Find the video device of the notifier tree that this notifier is part of */
static struct video_device *video_async_find_vdev(struct video_notifier *notifier)
{
	if (!notifier) {
		return NULL;
	}

	while (notifier->parent) {
		notifier = notifier->parent;
	}

	return notifier->vdev;
}

void video_async_init(struct video_notifier *notifier, const struct device *dev,
		      struct video_device *vdev, const struct device **children_devs,
		      uint8_t children_num)
{
	if (!notifier) {
		return;
	}

	notifier->dev = dev;
	notifier->vdev = vdev;
	notifier->children_devs = (const struct device **)children_devs;
	notifier->children_num = children_num;
}

int video_async_register(struct video_notifier *notifier)
{
	uint8_t i = 0;
	bool found_parent = false;
	struct video_notifier *nf;
	struct video_device *vdev = NULL;

	if (!notifier) {
		return -EINVAL;
	}

	if (sys_slist_find(&notifiers_list, &notifier->node, NULL)) {
		LOG_ERR("Notifier of device %s is already registered", notifier->dev->name);
		return -EALREADY;
	}

	SYS_SLIST_FOR_EACH_CONTAINER(&notifiers_list, nf, node) {
		/* Find the parent notifier in the global notifiers registry */
		if (!found_parent) {
			for (i = 0; i < nf->children_num; ++i) {
				if (nf->children_devs[i] == notifier->dev) {
					notifier->parent = nf;

					vdev = video_async_find_vdev(notifier);
					if (vdev) {
						notifier->vdev = vdev;
					}

					found_parent = true;
					break;
				}
			}
		}

		/* Find all children notifiers in the notifiers registry */
		for (i = 0; i < notifier->children_num; ++i) {
			if (notifier->children_devs[i] == nf->dev) {
				nf->parent = notifier;
				break;
			}
		}
	}

	/* Add the notifier to the notifiers registry */
	sys_slist_append(&notifiers_list, &notifier->node);

	LOG_DBG("Notifier of device %s is registered", notifier->dev->name);

	return 0;
}

void video_async_unregister(struct video_notifier *notifier)
{
	if (!notifier) {
		return;
	}

	sys_slist_find_and_remove(&notifiers_list, &notifier->node);
}
