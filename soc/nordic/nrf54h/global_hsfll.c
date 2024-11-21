/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>

static struct onoff_client cli;
static const struct device *global_hsfll = DEVICE_DT_GET(DT_NODELABEL(hsfll120));
static const struct nrf_clock_spec spec = {
	.frequency = CONFIG_SOC_NRF54H20_GDFS_MIN_FREQ,
};

static int nordicsemi_nrf54h_gdfs_init(void)
{
	int ret;
	int res;

	sys_notify_init_spinwait(&cli.notify);

	ret = nrf_clock_control_request(clk_dev, clk_spec, &cli);
	if (ret) {
		return ret;
	}

	ret = WAIT_FOR(sys_notify_fetch_result(&cli.notify, &res) == 0,
		       2000,
		       k_msleep(1);
	);

	if (ret) {
		return ret;
	}

	return res;
}

SYS_INIT(nordicsemi_nrf54h_gdfs_init, POST_KERNEL, 99);
