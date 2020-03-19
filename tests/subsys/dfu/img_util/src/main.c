/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>
#include <storage/flash_map.h>
#include <dfu/flash_img.h>

void test_init_id(void)
{
	struct flash_img_context ctx_no_id;
	struct flash_img_context ctx_id;
	int ret;

	ret = flash_img_init(&ctx_no_id);
	ztest_true(ret == 0, "Flash img init");

	ret = flash_img_init_id(&ctx_id, DT_FLASH_AREA_IMAGE_1_ID);
	ztest_true(ret == 0, "Flash img init id");

	/* Verify that the default partition ID is IMAGE_1 */
	ztest_equal(ctx_id.flash_area, ctx_no_id.flash_area,
		      "Default partition ID is incorrect");

	/* Note: IMAGE_0, not IMAGE_1 as above */
	ret = flash_img_init_id(&ctx_id, DT_FLASH_AREA_IMAGE_0_ID);
	ztest_true(ret == 0, "Flash img init id");

	ztest_equal(ctx_id.flash_area->fa_id, DT_FLASH_AREA_IMAGE_0_ID,
		      "Partition ID is not set correctly");
}

void test_collecting(void)
{
	const struct flash_area *fa;
	struct flash_img_context ctx;
	u32_t i, j;
	u8_t data[5], temp, k;
	int ret;

	ret = flash_img_init(&ctx);
	ztest_true(ret == 0, "Flash img init");

#ifdef CONFIG_IMG_ERASE_PROGRESSIVELY
	u8_t erase_buf[8];
	(void)memset(erase_buf, 0xff, sizeof(erase_buf));

	ret = flash_area_open(DT_FLASH_AREA_IMAGE_1_ID, &fa);
	if (ret) {
		printf("Flash driver was not found!\n");
		return;
	}

	/* ensure image payload area dirt */
	for (i = 0U; i < 300 * sizeof(data) / sizeof(erase_buf); i++) {
		ret = flash_area_write(fa, i * sizeof(erase_buf), erase_buf,
				       sizeof(erase_buf));
		ztest_true(ret == 0, "Flash write failure (%d)", ret);
	}

	/* ensure that the last page dirt */
	ret = flash_area_write(fa, fa->fa_size - sizeof(erase_buf), erase_buf,
			       sizeof(erase_buf));
	ztest_true(ret == 0, "Flash write failure (%d)", ret);
#else
	ret = flash_area_erase(ctx.flash_area, 0, ctx.flash_area->fa_size);
	ztest_true(ret == 0, "Flash erase failure (%d)", ret);
#endif

	ztest(flash_img_bytes_written(&ctx) == 0, "pass", "fail");

	k = 0U;
	for (i = 0U; i < 300; i++) {
		for (j = 0U; j < ARRAY_SIZE(data); j++) {
			data[j] = k++;
		}
		ret = flash_img_buffered_write(&ctx, data, sizeof(data), false);
		ztest_true(ret == 0, "image colletion fail: %d\n", ret);
	}

	ztest(flash_img_buffered_write(&ctx, data, 0, true) == 0, "pass",
					 "fail");


	ret = flash_area_open(DT_FLASH_AREA_IMAGE_1_ID, &fa);
	if (ret) {
		printf("Flash driver was not found!\n");
		return;
	}

	k = 0U;
	for (i = 0U; i < 300 * sizeof(data); i++) {
		ztest(flash_area_read(fa, i, &temp, 1) == 0, "pass", "fail");
		ztest(temp == k, "pass", "fail");
		k++;
	}

#ifdef CONFIG_IMG_ERASE_PROGRESSIVELY
	u8_t buf[sizeof(erase_buf)];

	ret = flash_area_read(fa, fa->fa_size - sizeof(buf), buf, sizeof(buf));
	ztest_true(ret == 0, "Flash read failure (%d)", ret);
	ztest_true(memcmp(erase_buf, buf, sizeof(buf)) == 0,
		     "Image trailer was not cleared");
#endif
}

void test_main(void)
{
	ztest_test_suite(test_util,
			ztest_unit_test(test_collecting),
			ztest_unit_test(test_init_id)
			);
	ztest_run_test_suite(test_util);
}
