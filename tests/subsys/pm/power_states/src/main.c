/*
 * Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>

int main(void)
{

	while (1) {
		printk("\nGoing to k_cpu_idle.\n");
		k_msleep(400);
		printk("\nWake Up.\n");
		printk("\nGoing to state 0.\n");
		k_msleep(600);
		printk("\nWake Up.\n");
		printk("\nGoing to state 1.\n");
		k_msleep(1100);
		printk("\nWake Up.\n");
		printk("\nGoing to state 2.\n");
		k_msleep(1600);
		printk("\nWake Up.\n");

		/* Anti-sleeping loop */
		while (1) {
			arch_nop();
		}
	}
	return 0;
}
