/*
 * Copyright (c) 2018 qianfan Zhao
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_PINMUX_STM32_PINMUX_STM32F2_H_
#define ZEPHYR_DRIVERS_PINMUX_STM32_PINMUX_STM32F2_H_

/**
 * @file Header for STM32F2 pin multiplexing helper
 */

/*
 * Note:
 * The SPIx_SCK pin speed must be set to VERY_HIGH to avoid last data bit
 * corruption which is a known issue of STM32F2 SPI peripheral (see errata
 * sheets).
 */

/* Port A */
#define STM32F2_PINMUX_FUNC_PA0_UART4_TX __DEPRECATED_MACRO    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)
#define STM32F2_PINMUX_FUNC_PA1_ETH __DEPRECATED_MACRO         \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)
#define STM32F2_PINMUX_FUNC_PA0_ADC123_IN0 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PA1_UART4_RX __DEPRECATED_MACRO    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_NOPULL)
#define STM32F2_PINMUX_FUNC_PA1_ADC123_IN1 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PA2_USART2_TX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)
#define STM32F2_PINMUX_FUNC_PA2_ETH __DEPRECATED_MACRO         \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)
#define STM32F2_PINMUX_FUNC_PA2_ADC123_IN2 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PA3_USART2_RX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_NOPULL)
#define STM32F2_PINMUX_FUNC_PA3_ETH __DEPRECATED_MACRO         \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)
#define STM32F2_PINMUX_FUNC_PA3_ADC123_IN3 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PA4_ADC12_IN4 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE
#define STM32F2_PINMUX_FUNC_PA4_DAC_OUT1 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PA5_ADC12_IN5 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE
#define STM32F2_PINMUX_FUNC_PA5_DAC_OUT2 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PA6_ADC12_IN6 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PA7_ETH __DEPRECATED_MACRO         \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)
#define STM32F2_PINMUX_FUNC_PA7_ADC12_IN7 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PA9_USART1_TX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)

#define STM32F2_PINMUX_FUNC_PA10_USART1_RX __DEPRECATED_MACRO  \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_NOPULL)

#define STM32F2_PINMUX_FUNC_PA11_OTG_FS_DM  \
	(STM32_PINMUX_ALT_FUNC_10 | STM32_PUSHPULL_NOPULL)

#define STM32F2_PINMUX_FUNC_PA12_OTG_FS_DP  \
	(STM32_PINMUX_ALT_FUNC_10 | STM32_PUSHPULL_NOPULL)

/* Port B */
#define STM32F2_PINMUX_FUNC_PB0_ADC12_IN8 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PB1_ADC12_IN9 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PB6_USART1_TX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)

#define STM32F2_PINMUX_FUNC_PB7_USART1_RX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_NOPULL)

#define STM32F2_PINMUX_FUNC_PB10_USART3_TX __DEPRECATED_MACRO  \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)

#define STM32F2_PINMUX_FUNC_PB11_USART3_RX __DEPRECATED_MACRO  \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_NOPULL)
#define STM32F2_PINMUX_FUNC_PB11_ETH __DEPRECATED_MACRO        \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F2_PINMUX_FUNC_PB12_ETH __DEPRECATED_MACRO        \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F2_PINMUX_FUNC_PB13_ETH __DEPRECATED_MACRO        \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

/* Port C */
#define STM32F2_PINMUX_FUNC_PC0_ADC123_IN10 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PC1_ETH __DEPRECATED_MACRO         \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)
#define STM32F2_PINMUX_FUNC_PC1_ADC123_IN11 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PC2_ADC123_IN12 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PC3_ADC123_IN13 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PC4_ETH __DEPRECATED_MACRO         \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)
#define STM32F2_PINMUX_FUNC_PC4_ADC12_IN14 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PC5_ETH __DEPRECATED_MACRO         \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)
#define STM32F2_PINMUX_FUNC_PC5_ADC12_IN15 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

#define STM32F2_PINMUX_FUNC_PC6_USART6_TX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F2_PINMUX_FUNC_PC7_USART6_RX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_NOPULL)

#define STM32F2_PINMUX_FUNC_PC10_USART3_TX __DEPRECATED_MACRO  \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)
#define STM32F2_PINMUX_FUNC_PC10_UART4_TX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F2_PINMUX_FUNC_PC11_USART3_RX __DEPRECATED_MACRO  \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_NOPULL)
#define STM32F2_PINMUX_FUNC_PC11_UART4_RX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_NOPULL)

#define STM32F2_PINMUX_FUNC_PC12_UART5_TX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

/* Port D */
#define STM32F2_PINMUX_FUNC_PD2_UART5_RX __DEPRECATED_MACRO    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_NOPULL)

#define STM32F2_PINMUX_FUNC_PD5_USART2_TX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)

#define STM32F2_PINMUX_FUNC_PD6_USART2_RX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_NOPULL)

#define STM32F2_PINMUX_FUNC_PD8_USART3_TX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)

#define STM32F2_PINMUX_FUNC_PD9_USART3_RX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_NOPULL)

/* Port E */

/* Port F */
#define STM32F2_PINMUX_FUNC_PF3_ADC3_IN9 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE
#define STM32F2_PINMUX_FUNC_PF4_ADC3_IN14 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE
#define STM32F2_PINMUX_FUNC_PF5_ADC3_IN15 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE
#define STM32F2_PINMUX_FUNC_PF6_ADC3_IN4 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE
#define STM32F2_PINMUX_FUNC_PF7_ADC3_IN5 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE
#define STM32F2_PINMUX_FUNC_PF8_ADC3_IN6 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE
#define STM32F2_PINMUX_FUNC_PF9_ADC3_IN7 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE
#define STM32F2_PINMUX_FUNC_PF10_ADC3_IN8 __DEPRECATED_MACRO	\
	STM32_MODER_ANALOG_MODE

/* Port G */
#define STM32F2_PINMUX_FUNC_PG9_USART6_RX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_NOPULL)

#define STM32F2_PINMUX_FUNC_PG11_ETH __DEPRECATED_MACRO        \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F2_PINMUX_FUNC_PG13_ETH __DEPRECATED_MACRO        \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F2_PINMUX_FUNC_PG14_ETH __DEPRECATED_MACRO        \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)
#define STM32F2_PINMUX_FUNC_PG14_USART6_TX __DEPRECATED_MACRO   \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

/* Port H */

#endif /* ZEPHYR_DRIVERS_PINMUX_STM32_PINMUX_STM32F2_H_ */
