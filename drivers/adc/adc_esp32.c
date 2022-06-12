/*
 * Copyright (c) 2022 Wolter HELLMUND VEGA <wolterhv@gmx.de>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * adc_stm32.c has been used as a reference for this implementation.
 */

#define DT_DRV_COMPAT espressif_esp32_adc

#include <errno.h>

#include <drivers/adc.h>

/* From espressif */
#include "hal/adc_hal.h"
#include "hal/adc_hal_conf.h"

#define LOG_LEVEL CONFIG_ADC_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(adc_esp32);

struct adc_esp32_cfg;

static int adc_esp32_init          (const struct device *dev);
static int adc_esp32_channel_setup (const struct device *dev,
				    const struct adc_channel_cfg *channel_cfg);
static int adc_esp32_read          (const struct device *dev,
			            const struct adc_sequence *sequence);
#ifdef CONFIG_ADC_ASYNC
static int adc_esp32_read_async    (const struct device *dev,
			            const struct adc_sequence *sequence,
			            struct k_poll_signal *async);
#endif /* CONFIG_ADC_ASYNC */

static const struct adc_driver_api api_esp32_driver_api = {
	.channel_setup = adc_esp32_channel_setup,
	.read          = adc_esp32_read,
#ifdef CONFIG_ADC_ASYNC
	.read_async    = adc_esp32_read_async,
#endif /* CONFIG_ADC_ASYNC */
};

/* Implementation */

static int adc_esp32_init(const struct device *dev)
{
	adc_hal_init();
	return 0;
}

static int adc_esp32_channel_setup (const struct device *dev,
				    const struct adc_channel_cfg *channel_cfg)
{
#error "Not implemented"
	adc1_config_width();
	adc1_config_channel_atten();
	return 0;
}

/*
 * Channels, buffer and resolution are in the sequence param.
 * Other device-specific stuff is retrieved from the dev param.
 * Reads samples for the channels specified in the sequence struct.
 * Stores one sample per channel on the sequence struct buffer.
 */
static int adc_esp32_read          (const struct device *dev,
			            const struct adc_sequence *sequence)
{
	int error;

	/* adc1_get_raw(); */
	/* adc2_get_raw(); */
	/* Set resolution, according to channel */
	sequence->buffer[chan_idx] = raw_value;

	return error;
}

#ifdef CONFIG_ADC_ASYNC
static int adc_esp32_read_async    (const struct device *dev,
			            const struct adc_sequence *sequence,
			            struct k_poll_signal *async)
{
#error "Not implemented"
	return 0;
}
#endif /* CONFIG_ADC_ASYNC */


/* Footer */

#define ADC_ESP32_CONFIG(index)						\
static const struct adc_esp32_cfg adc_esp32_cfg_##index = {		\
};

#define ESP32_ADC_INIT(index)						\
									\
PINCTRL_DT_INST_DEFINE(index);						\
									\
ADC_ESP32_CONFIG(index)							\
									\
static struct adc_esp32_data adc_esp32_data_##index = {			\
	ADC_CONTEXT_INIT_TIMER (adc_esp32_data_##index, ctx),		\
	ADC_CONTEXT_INIT_LOCK  (adc_esp32_data_##index, ctx),		\
	ADC_CONTEXT_INIT_SYNC  (adc_esp32_data_##index, ctx),		\
};									\
									\
DEVICE_DT_INST_DEFINE(index,						\
		      &adc_esp32_init, NULL,				\
		      &adc_esp32_data_##index, &adc_esp32_cfg_##index	\
		      POST_KERNEL, CONFIG_ADC_INIT_PRIORITY,		\
		      &api_esp32_driver_api);				\

DT_INST_FOREACH_STATUS_OKAY(ESP32_ADC_INIT)
