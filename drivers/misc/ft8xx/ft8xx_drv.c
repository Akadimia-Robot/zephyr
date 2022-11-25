/*
 * Copyright (c) 2020 Hubert Miś
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ft8xx_drv.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <string.h>

#define LOG_MODULE_NAME ft8xx_drv
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define DT_DRV_COMPAT ftdi_ft800
//#define NODE_ID DT_INST(0, DT_DRV_COMPAT)

/* SPI device */
/*
static const struct spi_dt_spec spi = SPI_DT_SPEC_INST_GET(0,
					 SPI_WORD_SET(8) | SPI_OP_MODE_MASTER,
					 0);
*/


/* GPIO int line */
/*
static const struct gpio_dt_spec irq_gpio = GPIO_DT_SPEC_INST_GET(0, irq_gpios);
*/

__weak void ft8xx_drv_irq_triggered(const struct device *dev,
				     struct gpio_callback *cb, uint32_t pins)
{
	/* Intentionally empty */
}

/* Protocol details */
#define ADDR_SIZE 3
#define DUMMY_READ_SIZE 1
#define COMMAND_SIZE 3
#define MAX_READ_LEN (UINT16_MAX - ADDR_SIZE - DUMMY_READ_SIZE)
#define MAX_WRITE_LEN (UINT16_MAX - ADDR_SIZE)

#define READ_OP 0x00
#define WRITE_OP 0x80
#define COMMAND_OP 0x40

static void insert_addr(uint32_t addr, uint8_t *buff)
{
	buff[0] = (addr >> 16) & 0x3f;
	buff[1] = (addr >> 8) & 0xff;
	buff[2] = (addr) & 0xff;
}

int ft8xx_drv_init(const struct device *dev)
{
	const struct ft8xx_config *config = dev->config;
	const struct ft8xx_data *data = dev->data;
	
	

	static struct gpio_callback irq_cb_data;

	int ret;

	if (!spi_is_ready(config->bus->spi)) {
		LOG_ERR("SPI bus %s not ready", config->bus->spi.bus->name);
		return -ENODEV;
	}

	/* TODO: Verify if such entry in DTS is present.
	 * If not, use polling mode.
	 */
	if (!device_is_ready(config->irq_gpio.port)) {
		LOG_ERR("GPIO device %s is not ready", config->irq_gpio.port->name);
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(config->irq_gpio, GPIO_INPUT);
	if (ret != 0) {
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(config->irq_gpio, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		return ret;
	}

	gpio_init_callback(&irq_cb_data, ft8xx_drv_irq_triggered, BIT(config->irq_gpio.pin));
	gpio_add_callback(config->irq_gpio.port, &irq_cb_data);

	return 0;
}

static int ft8xx_bus_check_spi(const union ft8xx_bus *bus)
{
	return spi_is_ready(bus->spi) ? 0 : -ENODEV;
}


int ft8xx_drv_write(const union ft8xx_bus *bus, uint32_t address, const uint8_t *data, unsigned int length)
{
	int ret;
	uint8_t addr_buf[ADDR_SIZE];

	insert_addr(address, addr_buf);
	addr_buf[0] |= WRITE_OP;

	struct spi_buf tx[] = {
		{
			.buf = addr_buf,
			.len = sizeof(addr_buf),
		},
		{
			/* Discard const, it is implicit for TX buffer */
			.buf = (uint8_t *)data,
			.len = length,
		},
	};

	struct spi_buf_set tx_bufs = {
		.buffers = tx,
		.count = 2,
	};

	ret = spi_write_dt(bus->spi, &tx_bufs);
	if (ret < 0) {
		LOG_ERR("SPI write error: %d", ret);
	}

	return ret;
}

int ft8xx_drv_write_dual(const union ft8xx_bus *bus, uint32_t address, const uint8_t *data, unsigned int length,
				const uint8_t *data2, 
				unsigned int data2_length, 
				uint8_t padsize)
{
	int ret;
	uint8_t addr_buf[ADDR_SIZE];
	uint8_t pad[padsize]; 

	insert_addr(address, addr_buf);
	addr_buf[0] |= WRITE_OP;

	struct spi_buf tx[] = {
		{
			.buf = addr_buf,
			.len = sizeof(addr_buf),
		},
		{
			/* Discard const, it is implicit for TX buffer */
			.buf = (uint8_t *)data,
			.len = length,
		},
		{
			/* Discard const, it is implicit for TX buffer */
			.buf = (uint8_t *)data2,
			.len = data2_length,
		},
		{
			/* Discard const, it is implicit for TX buffer */
			.buf = pad,
			.len = padsize,
		},

	};

	struct spi_buf_set tx_bufs = {
		.buffers = tx,
		.count = 4,
	};



	ret = spi_write_dt(bus->spi, &tx_bufs);
	if (ret < 0) {
		LOG_ERR("SPI write error: %d", ret);
	}

	return ret;
}

int ft8xx_drv_read(const union ft8xx_bus *bus, uint32_t address, uint8_t *data, unsigned int length)
{
	int ret;
	uint8_t dummy_read_buf[ADDR_SIZE + DUMMY_READ_SIZE];
	uint8_t addr_buf[ADDR_SIZE];

	insert_addr(address, addr_buf);
	addr_buf[0] |= READ_OP;

	struct spi_buf tx = {
		.buf = addr_buf,
		.len = sizeof(addr_buf),
	};

	struct spi_buf_set tx_bufs = {
		.buffers = &tx,
		.count = 1,
	};

	struct spi_buf rx[] = {
		{
			.buf = dummy_read_buf,
			.len = sizeof(dummy_read_buf),
		},
		{
			.buf = data,
			.len = length,
		},
	};

	struct spi_buf_set rx_bufs = {
		.buffers = rx,
		.count = 2,
	};

	ret = spi_transceive_dt(bus->spi, &tx_bufs, &rx_bufs);
	if (ret < 0) {
		LOG_ERR("SPI transceive error: %d", ret);
	}

	return ret;
}

int ft8xx_drv_command(const union ft8xx_bus *bus, uint8_t command)
{
	int ret;
	/* Most commands include COMMAND_OP bit. ACTIVE power mode command is
	 * an exception with value 0x00.
	 */
	uint8_t cmd_buf[COMMAND_SIZE] = {command, 0, 0};

	struct spi_buf tx = {
		.buf = cmd_buf,
		.len = sizeof(cmd_buf),
	};

	struct spi_buf_set tx_bufs = {
		.buffers = &tx,
		.count = 1,
	};

	ret = spi_write_dt(bus->spi, &tx_bufs);
	if (ret < 0) {
		LOG_ERR("SPI command error: %d", ret);
	}

	return ret;

const struct ft8xx_bus_io ft8xx_bus_io_spi = {
	.check = ft8xx_bus_check_spi,
	.read = ft8xx_drv_read_spi,
	.write = ft8xx_drv_write_spi,
};


}
