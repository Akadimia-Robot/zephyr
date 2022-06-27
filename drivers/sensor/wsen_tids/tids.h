/*
 * Copyright (c) 2022 Würth Elektronik eiSos GmbH & Co. KG
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_TIDS_TIDS_H_
#define ZEPHYR_DRIVERS_SENSOR_TIDS_TIDS_H_

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>

#include <weplatform.h>

#include "WSEN_TIDS_2521020222501.h"

#if DT_ANY_INST_ON_BUS_STATUS_OKAY(i2c)
#include <drivers/i2c.h>
#endif /* DT_ANY_INST_ON_BUS_STATUS_OKAY(i2c) */

struct tids_data {
	/* WE sensor interface configuration */
	WE_sensorInterface_t sensor_interface;

	/* Last temperature sample */
	int16_t temperature;

#ifdef CONFIG_TIDS_TRIGGER
	const struct device *dev;

	/* Callback for high/low limit interrupts */
	struct gpio_callback threshold_cb;

	struct sensor_trigger threshold_trigger;
	sensor_trigger_handler_t threshold_handler;

#if defined(CONFIG_TIDS_TRIGGER_OWN_THREAD)
	K_KERNEL_STACK_MEMBER(thread_stack, CONFIG_TIDS_THREAD_STACK_SIZE);
	struct k_thread thread;
	struct k_sem threshold_sem;
#elif defined(CONFIG_TIDS_TRIGGER_GLOBAL_THREAD)
	struct k_work work;
#endif
#endif /* CONFIG_TIDS_TRIGGER */
};

struct tids_config {
	union {
#if DT_ANY_INST_ON_BUS_STATUS_OKAY(i2c)
		const struct i2c_dt_spec i2c;
#endif
	} bus_cfg;

#ifdef CONFIG_TIDS_TRIGGER
	/* Interrupt pin used for high and low limit interrupt events */
	const struct gpio_dt_spec gpio_threshold;
#endif
};

#ifdef CONFIG_TIDS_TRIGGER
int tids_trigger_set(const struct device *dev, const struct sensor_trigger *trig,
		     sensor_trigger_handler_t handler);

int tids_threshold_set(const struct device *dev, const struct sensor_value *thresh_value,
		       bool upper);

int tids_init_interrupt(const struct device *dev);
#endif

int tids_i2c_init(const struct device *dev);

#endif /* ZEPHYR_DRIVERS_SENSOR_TIDS_TIDS_H_ */
