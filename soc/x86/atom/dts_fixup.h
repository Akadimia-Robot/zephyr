/* SPDX-License-Identifier: Apache-2.0 */

/* SoC level DTS fixup file */

#define DT_UART_NS16550_PORT_0_BASE_ADDR	DT_NS16550_3F8_BASE_ADDRESS
#define DT_UART_NS16550_PORT_0_BAUD_RATE	DT_NS16550_3F8_CURRENT_SPEED
#define DT_UART_NS16550_PORT_0_NAME		DT_NS16550_3F8_LABEL
#define DT_UART_NS16550_PORT_0_IRQ		DT_NS16550_3F8_IRQ_0
#define DT_UART_NS16550_PORT_0_IRQ_PRI		DT_NS16550_3F8_IRQ_0_PRIORITY
#define DT_UART_NS16550_PORT_0_IRQ_FLAGS	DT_NS16550_3F8_IRQ_0_SENSE
#define DT_UART_NS16550_PORT_0_CLK_FREQ	DT_NS16550_3F8_CLOCK_FREQUENCY

#define DT_UART_NS16550_PORT_1_BASE_ADDR	DT_NS16550_2F8_BASE_ADDRESS
#define DT_UART_NS16550_PORT_1_BAUD_RATE	DT_NS16550_2F8_CURRENT_SPEED
#define DT_UART_NS16550_PORT_1_NAME		DT_NS16550_2F8_LABEL
#define DT_UART_NS16550_PORT_1_IRQ		DT_NS16550_2F8_IRQ_0
#define DT_UART_NS16550_PORT_1_IRQ_PRI		DT_NS16550_2F8_IRQ_0_PRIORITY
#define DT_UART_NS16550_PORT_1_IRQ_FLAGS	DT_NS16550_2F8_IRQ_0_SENSE
#define DT_UART_NS16550_PORT_1_CLK_FREQ	DT_NS16550_2F8_CLOCK_FREQUENCY

#define DT_PHYS_RAM_ADDR			CONFIG_SRAM_BASE_ADDRESS

#define DT_PHYS_LOAD_ADDR			CONFIG_FLASH_BASE_ADDRESS

#define DT_RAM_SIZE				CONFIG_SRAM_SIZE

#define DT_ROM_SIZE				CONFIG_FLASH_SIZE

/* End of SoC Level DTS fixup file */
