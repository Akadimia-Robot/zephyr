/* clock_control.h - public clock controller driver API */

/*
 * Copyright (c) 2015 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public Clock Control APIs
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_H_
#define ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_H_

/**
 * @brief Clock Control Interface
 * @defgroup clock_control_interface Clock Control Interface
 * @ingroup io_interfaces
 * @{
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <device.h>
#include <sys/__assert.h>
#include <sys/slist.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Clock control API */

/* Used to select all subsystem of a clock controller */
#define CLOCK_CONTROL_SUBSYS_ALL	NULL

/**
 * @brief Current clock status.
 */
enum clock_control_status {
	CLOCK_CONTROL_STATUS_STARTING,
	CLOCK_CONTROL_STATUS_OFF,
	CLOCK_CONTROL_STATUS_ON,
	CLOCK_CONTROL_STATUS_UNAVAILABLE,
	CLOCK_CONTROL_STATUS_UNKNOWN
};

/**
 * clock_control_subsys_t is a type to identify a clock controller sub-system.
 * Such data pointed is opaque and relevant only to the clock controller
 * driver instance being used.
 */
typedef void *clock_control_subsys_t;

/** @brief Callback called on clock started.
 *
 * @param dev		Device structure whose driver controls the clock.
 * @param subsys	Opaque data representing the clock.
 * @param user_data	User data.
 */
typedef void (*clock_control_cb_t)(const struct device *dev,
				   clock_control_subsys_t subsys,
				   void *user_data);

typedef int (*clock_control)(const struct device *dev,
			     clock_control_subsys_t sys);

typedef int (*clock_control_get)(const struct device *dev,
				 clock_control_subsys_t sys,
				 uint32_t *rate);

typedef int (*clock_control_async_on_fn)(const struct device *dev,
					 clock_control_subsys_t sys,
					 clock_control_cb_t cb,
					 void *user_data);

typedef enum clock_control_status (*clock_control_get_status_fn)(
						    const struct device *dev,
						    clock_control_subsys_t sys);

struct clock_control_driver_api {
	clock_control			on;
	clock_control			off;
	clock_control_async_on_fn	async_on;
	clock_control_get		get_rate;
	clock_control_get_status_fn	get_status;
};

/**
 * @brief Enable a clock controlled by the device
 *
 * On success, the clock is enabled and ready when this function
 * returns. This function may sleep, and thus can only be called from
 * thread context.
 *
 * Use @ref clock_control_async_on() for non-blocking operation.
 *
 * @param dev Device structure whose driver controls the clock.
 * @param sys Opaque data representing the clock.
 * @return 0 on success, negative errno on failure.
 */
static inline int clock_control_on(const struct device *dev,
				   clock_control_subsys_t sys)
{
	int ret = device_usable_check(dev);

	if (ret != 0) {
		return ret;
	}

	const struct clock_control_driver_api *api =
		(const struct clock_control_driver_api *)dev->api;

	return api->on(dev, sys);
}

/**
 * @brief Disable a clock controlled by the device
 *
 * This function is non-blocking and can be called from any context.
 * On success, the clock is disabled when this function returns.
 *
 * @param dev Device structure whose driver controls the clock
 * @param sys Opaque data representing the clock
 * @return 0 on success, negative errno on failure.
 */
static inline int clock_control_off(const struct device *dev,
				    clock_control_subsys_t sys)
{
	int ret = device_usable_check(dev);

	if (ret != 0) {
		return ret;
	}

	const struct clock_control_driver_api *api =
		(const struct clock_control_driver_api *)dev->api;

	return api->off(dev, sys);
}

/**
 * @brief Request clock to start with notification when clock has been started.
 *
 * Function is non-blocking and can be called from any context. User callback is
 * called when clock is started.
 *
 * @param dev	    Device.
 * @param sys	    A pointer to an opaque data representing the sub-system.
 * @param cb	    Callback.
 * @param user_data User context passed to the callback.
 *
 * @retval 0 if start is successfully initiated.
 * @retval -EALREADY if clock was already started and is starting or running.
 * @retval -ENOTSUP if not supported.
 * @retval other negative errno on vendor specific error.
 */
static inline int clock_control_async_on(const struct device *dev,
					 clock_control_subsys_t sys,
					 clock_control_cb_t cb,
					 void *user_data)
{
	const struct clock_control_driver_api *api =
		(const struct clock_control_driver_api *)dev->api;

	if (!api->async_on) {
		return -ENOTSUP;
	}

	int ret = device_usable_check(dev);

	if (ret != 0) {
		return ret;
	}

	return api->async_on(dev, sys, cb, user_data);
}

/**
 * @brief Get clock status.
 *
 * @param dev Device.
 * @param sys A pointer to an opaque data representing the sub-system.
 *
 * @return Status.
 */
static inline enum clock_control_status clock_control_get_status(const struct device *dev,
								 clock_control_subsys_t sys)
{
	const struct clock_control_driver_api *api =
		(const struct clock_control_driver_api *)dev->api;

	if (!api->get_status) {
		return CLOCK_CONTROL_STATUS_UNKNOWN;
	}

	if (!device_is_ready(dev)) {
		return CLOCK_CONTROL_STATUS_UNAVAILABLE;
	}

	return api->get_status(dev, sys);
}

/**
 * @brief Obtain the clock rate of given sub-system
 * @param dev Pointer to the device structure for the clock controller driver
 *        instance
 * @param sys A pointer to an opaque data representing the sub-system
 * @param[out] rate Subsystem clock rate
 */
static inline int clock_control_get_rate(const struct device *dev,
					 clock_control_subsys_t sys,
					 uint32_t *rate)
{
	int ret = device_usable_check(dev);

	if (ret != 0) {
		return ret;
	}

	const struct clock_control_driver_api *api =
		(const struct clock_control_driver_api *)dev->api;

	__ASSERT(api->get_rate != NULL, "%s not implemented for device %s",
		__func__, dev->name);

	return api->get_rate(dev, sys, rate);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_H_ */
