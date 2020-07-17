/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __PIN_CONFIG_STM32_H__
#define __PIN_CONFIG_STM32_H__

#define DT_ILITEK_ILI9340_0_BUS_NAME "SPI_1"
#define DT_ILITEK_ILI9340_0_SPI_MAX_FREQUENCY (24*1000*1000)

#define DT_ILITEK_ILI9340_0_BASE_ADDRESS      1
#define DT_ILITEK_ILI9340_0_RESET_GPIOS_CONTROLLER "GPIOC"
#define DT_ILITEK_ILI9340_0_RESET_GPIOS_PIN    12
#define DT_ILITEK_ILI9340_0_CMD_DATA_GPIOS_CONTROLLER "GPIOC"
#define DT_ILITEK_ILI9340_0_CMD_DATA_GPIOS_PIN 11

#define DT_ILITEK_ILI9340_0_CS_GPIO_CONTROLLER  "GPIOC"
#define DT_ILITEK_ILI9340_0_CS_GPIO_PIN            10

#define XPT2046_SPI_DEVICE_NAME "SPI_1"
#define XPT2046_SPI_MAX_FREQUENCY (12*1000*1000)
#define XPT2046_CS_GPIO_CONTROLLER "GPIOD"
#define XPT2046_CS_GPIO_PIN         0

#define XPT2046_PEN_GPIO_CONTROLLER "GPIOD"
#define XPT2046_PEN_GPIO_PIN         1

#define HOST_DEVICE_COMM_UART_NAME "UART_6"

#endif /* __PIN_CONFIG_STM32_H__ */
