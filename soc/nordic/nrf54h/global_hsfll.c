/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(soc, CONFIG_SOC_LOG_LEVEL);

#define GLOBAL_HSFLL_MIN_FREQ_HZ \
	CONFIG_SOC_NRF54H20_GLOBAL_HSFLL_MIN_FREQ_HZ

#define GLOBAL_HSFLL_REQUEST_TIMEOUT_MS \
	CONFIG_SOC_NRF54H20_GLOBAL_HSFLL_TIMEOUT_MS

static struct onoff_client cli;
static const struct device *global_hsfll = DEVICE_DT_GET(DT_NODELABEL(hsfll120));
static const struct nrf_clock_spec spec = {
	.frequency = GLOBAL_HSFLL_MIN_FREQ_HZ,
};

static int nordicsemi_nrf54h_global_hsfll_init(void)
{
	int ret;
	int res;
	bool completed;

	sys_notify_init_spinwait(&cli.notify);

	ret = nrf_clock_control_request(global_hsfll, &spec, &cli);
	if (ret) {
		return ret;
	}

	res = -EIO;
	completed = WAIT_FOR(sys_notify_fetch_result(&cli.notify, &res) == 0,
			     GLOBAL_HSFLL_REQUEST_TIMEOUT_MS,
			     k_msleep(1));

	if (!completed || res) {
		LOG_ERR("Failed to restrict global HSFLL frequency");
		return -EIO;
	}

	return 0;
}

SYS_INIT(nordicsemi_nrf54h_global_hsfll_init, POST_KERNEL, 99);
