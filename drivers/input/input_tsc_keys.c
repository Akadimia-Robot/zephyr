/*
 * Copyright (c) 2024 Arif Balik <arifbalik@outlook.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT tsc_keys

#include <autoconf.h>
#include <stdbool.h>
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/drivers/misc/stm32_tsc/stm32_tsc.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/spsc_lockfree.h>
#include <zephyr/sys/spsc_pbuf.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/ring_buffer.h>

LOG_MODULE_REGISTER(tsc_keys, CONFIG_INPUT_LOG_LEVEL);

struct input_tsc_keys_child_data {
	uint32_t buffer[CONFIG_INPUT_TSC_KEYS_BUFFER_WORD_SIZE];
	struct ring_buf rb;
	enum {
		INPUT_TSC_KEYS_EXPECT_DOWN,
		INPUT_TSC_KEYS_EXPECT_UP,
	} state;
};

struct input_tsc_keys_child_config {
	char *label;
	uint8_t group_index;
	int32_t noise_threshold;
	int zephyr_code;
	struct input_tsc_keys_child_data *data;
};

struct input_tsc_keys_data {
	struct k_timer sampling_timer;
};

struct input_tsc_keys_config {
	const struct device *tsc_dev;
	stm32_tsc_callback_t tsc_callback;
	uint32_t sampling_interval_ms;
	struct input_tsc_keys_child_config *children;
	uint8_t child_num;
};

static void input_tsc_sampling_timer_callback(struct k_timer *timer)
{
	const struct device *dev = k_timer_user_data_get(timer);

	stm32_tsc_start(dev);
}

static inline struct input_tsc_keys_child_config *
input_tsc_child_config_get(const struct device *dev, uint8_t group)
{
	const struct input_tsc_keys_config *config = dev->config;

	for (int i = 0; i < config->child_num; i++) {
		const struct input_tsc_keys_child_config *child = &config->children[i];

		if (child->group_index == group) {
			return (struct input_tsc_keys_child_config *)child;
		}
	}

	return NULL;
}

static void input_tsc_callback_handler(const struct device *dev, uint8_t group, uint32_t value)
{
	const struct input_tsc_keys_config *config = dev->config;

	const struct input_tsc_keys_child_config *child_config =
		input_tsc_child_config_get(dev, group);

	if (!child_config) {
		LOG_ERR("TSC@%p: No child config for group %d", config->tsc_dev, group);
		return;
	}

	struct input_tsc_keys_child_data *child_data = child_config->data;

	if (ring_buf_item_space_get(&child_data->rb) == 0) {
		uint32_t oldest_point;
		int32_t slope;

		(void)ring_buf_get(&child_data->rb, (uint8_t *)&oldest_point, sizeof(oldest_point));

		slope = value - oldest_point;
		if (slope < -child_config->noise_threshold &&
		    child_data->state == INPUT_TSC_KEYS_EXPECT_DOWN) {
			/* this is a key press, now expect a release */
			child_data->state = INPUT_TSC_KEYS_EXPECT_UP;
			input_report_key(dev, child_config->zephyr_code, 1, false, K_NO_WAIT);
		} else if (slope > child_config->noise_threshold &&
			   child_data->state == INPUT_TSC_KEYS_EXPECT_UP) {
			/* this is a key release, now expect a press */
			child_data->state = INPUT_TSC_KEYS_EXPECT_DOWN;
			input_report_key(dev, child_config->zephyr_code, 0, false, K_NO_WAIT);
		}
	}

	(void)ring_buf_put(&child_data->rb, (uint8_t *)&value, sizeof(value));
}

int input_tsc_keys_init(const struct device *dev)
{
	const struct input_tsc_keys_config *config = dev->config;
	struct input_tsc_keys_data *data = dev->data;

	if (!device_is_ready(config->tsc_dev)) {
		LOG_ERR("TSC@%p: TSC device not ready", config->tsc_dev);
		return -ENODEV;
	}

	for (uint8_t i = 0; i < config->child_num; i++) {
		const struct input_tsc_keys_child_config *child = &config->children[i];
		struct input_tsc_keys_child_data *child_data = child->data;

		ring_buf_item_init(&child_data->rb, CONFIG_INPUT_TSC_KEYS_BUFFER_WORD_SIZE,
				   child_data->buffer);
	}

	stm32_tsc_register_callback(config->tsc_dev, config->tsc_callback);

	k_timer_init(&data->sampling_timer, input_tsc_sampling_timer_callback, NULL);
	k_timer_user_data_set(&data->sampling_timer, (void *)config->tsc_dev);
	k_timer_start(&data->sampling_timer, K_NO_WAIT, K_MSEC(config->sampling_interval_ms));

	return 0;
}

#define TSC_KEYS_CHILD(node_id)                                                                    \
	{                                                                                          \
		.label = DT_PROP(node_id, label),                                                  \
		.group_index = DT_PROP(DT_PHANDLE(node_id, group), group),                         \
		.zephyr_code = DT_PROP(node_id, zephyr_code),                                      \
		.noise_threshold = DT_PROP(node_id, noise_threshold),                              \
		.data = &tsc_keys_child_data_##node_id,                                            \
	},

#define TSC_KEYS_CHILD_DATA(node_id)                                                               \
	static struct input_tsc_keys_child_data tsc_keys_child_data_##node_id;

#define TSC_KEYS_INIT(node_id)                                                                     \
	DT_INST_FOREACH_CHILD_STATUS_OKAY(node_id, TSC_KEYS_CHILD_DATA)                            \
	static struct input_tsc_keys_child_config tsc_keys_children_##node_id[] = {                \
		DT_INST_FOREACH_CHILD_STATUS_OKAY(node_id, TSC_KEYS_CHILD)};                       \
	static void stm32_tsc_callback_##node_id(uint8_t group, uint32_t value)                    \
	{                                                                                          \
		input_tsc_callback_handler(DEVICE_DT_INST_GET(node_id), group, value);             \
	}                                                                                          \
                                                                                                   \
	static struct input_tsc_keys_data tsc_keys_data_##node_id;                                 \
                                                                                                   \
	static const struct input_tsc_keys_config tsc_keys_config_##node_id = {                    \
		.tsc_dev = DEVICE_DT_GET(DT_INST_PHANDLE(node_id, tsc_controller)),                \
		.sampling_interval_ms = DT_INST_PROP(node_id, sampling_interval_ms),               \
		.child_num = DT_INST_CHILD_NUM_STATUS_OKAY(node_id),                               \
		.children = tsc_keys_children_##node_id,                                           \
		.tsc_callback = stm32_tsc_callback_##node_id,                                      \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(node_id, input_tsc_keys_init, NULL, &tsc_keys_data_##node_id,        \
			      &tsc_keys_config_##node_id, POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, \
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(TSC_KEYS_INIT);
