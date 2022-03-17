/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <drivers/flash.h>
#include <device.h>
#include <devicetree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if DT_NODE_HAS_STATUS(DT_INST(0, st_stm32_qspi_nand), okay)
#define FLASH_DEVICE DT_LABEL(DT_INST(0, st_stm32_qspi_nand))
#define FLASH_NAME "JEDEC QSPI-NOR"
#elif DT_NODE_HAS_STATUS(DT_INST(0, st_stm32_spi_nand), okay)
#define FLASH_DEVICE DT_LABEL(DT_INST(0, st_stm32_spi_nand))
#define FLASH_NAME "ST SPI-NAND"
#else
#error Unsupported flash driver
#endif

#if defined(CONFIG_BOARD_ADAFRUIT_FEATHER_STM32F405)
#define FLASH_TEST_REGION_OFFSET 0xf000
#elif defined(CONFIG_BOARD_ARTY_A7_ARM_DESIGNSTART_M1) || \
	defined(CONFIG_BOARD_ARTY_A7_ARM_DESIGNSTART_M3)
/* The FPGA bitstream is stored in the lower 536 sectors of the flash. */
#define FLASH_TEST_REGION_OFFSET \
	DT_REG_SIZE(DT_NODE_BY_FIXED_PARTITION_LABEL(fpga_bitstream))
#else
#define FLASH_TEST_REGION_OFFSET 0xff000
#endif
#define FLASH_BLOCK_SIZE        0x20000U

void main(void)
{
	static uint8_t expected[2048] = { 0 };
	const size_t len = sizeof(expected);
	static uint8_t buf[sizeof(expected)];
	const struct device *flash_dev;
	int rc;

	printf("\n" FLASH_NAME " SPI NAND Flash testing\n");
	printf("==========================\n");

	flash_dev = device_get_binding(FLASH_DEVICE);

	if (!flash_dev) {
		printf("SPI NAND Flash driver %s was not found!\n",
		       FLASH_DEVICE);
		return;
	}

	/* Write protection needs to be disabled before each write or
	 * erase, since the flash component turns on write protection
	 * automatically after completion of write and erase
	 * operations.
	 */
	printf("\nTest 1: Flash erase\n");

	rc = flash_erase(flash_dev, FLASH_TEST_REGION_OFFSET,
			 FLASH_BLOCK_SIZE);
	if (rc != 0) {
		printf("Flash erase failed! %d\n", rc);
	} else {
		printf("Flash erase succeeded!\n");
	}

	printf("\nTest 2: Flash write\n");

	for (int i = 0; i < sizeof(expected); i++) {
		expected[i] = (i % 0xFF);
	}
	printf("Attempting to write %u bytes\n", len);
	rc = flash_write(flash_dev, FLASH_TEST_REGION_OFFSET, expected, len);
	if (rc != 0) {
		printf("Flash write failed! %d\n", rc);
		return;
	}

	printf("\nTest 3: Flash read\n");
	memset(buf, 0, len);
	rc = flash_read(flash_dev, FLASH_TEST_REGION_OFFSET, buf, len);
	if (rc != 0) {
		printf("Flash read failed! %d\n", rc);
		return;
	}

	if (memcmp(expected, buf, len) == 0) {
		printf("Data read matches data written. Good!!\n");
	} else {
		const uint8_t *wp = expected;
		const uint8_t *rp = buf;
		const uint8_t *rpe = rp + len;

		printf("Data read does not match data written!!\n");
		while (rp < rpe) {
			printf("%08x wrote %02x read %02x %s\n",
			       (uint32_t)(FLASH_TEST_REGION_OFFSET + (rp - buf)),
			       *wp, *rp, (*rp == *wp) ? "match" : "MISMATCH");
			++rp;
			++wp;
		}
	}
}
