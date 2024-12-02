/*
 * Copyright (c) 2024 Arif Balik <arifbalik@outlook.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_STM32_TSC_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_STM32_TSC_H_

#include <zephyr/dt-bindings/dt-util.h>

#define TSC_PG_PRESC_DIV1   0x00000000UL
#define TSC_PG_PRESC_DIV2   0x1UL
#define TSC_PG_PRESC_DIV4   0x2UL
#define TSC_PG_PRESC_DIV8   0x3UL
#define TSC_PG_PRESC_DIV16  0x4UL
#define TSC_PG_PRESC_DIV32  0x5UL
#define TSC_PG_PRESC_DIV64  0x6UL
#define TSC_PG_PRESC_DIV128 0x7UL

#define TSC_MCV_255   0x00000000UL
#define TSC_MCV_511   0x1UL
#define TSC_MCV_1023  0x2UL
#define TSC_MCV_2047  0x3UL
#define TSC_MCV_4095  0x4UL
#define TSC_MCV_8191  0x5UL
#define TSC_MCV_16383 0x6UL

#define TSC_IO1 0x1UL
#define TSC_IO2 0x2UL
#define TSC_IO3 0x4UL
#define TSC_IO4 0x8UL

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_STM32_TSC_H_ */
