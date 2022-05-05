/*
 * Copyright (c) 2021 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>

__attribute__((noreturn))
void main(void)
{
	printk("Should not be printed, expected TF-M's NS application to be run instead.\n");
	k_panic();

	for (;;) {
	}
}
