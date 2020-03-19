/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "settings_test.h"
#include <device.h>
#include <fs/fs.h>
#include <fs/littlefs.h>

/* NFFS work area strcut */
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(cstorage);
static struct fs_mount_t littlefs_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &cstorage,
	.storage_dev = (void *)DT_FLASH_AREA_LITTLEFS_DEV_ID,
	.mnt_point = TEST_FS_MPTR,
};

void config_setup_littlefs(void)
{
	int rc;
	const struct flash_area *fap;

	rc = flash_area_open(DT_FLASH_AREA_LITTLEFS_DEV_ID, &fap);
	ztest_true(rc == 0, "opening flash area for erase [%d]\n", rc);

	flash_area_erase(fap, fap->fa_off, fap->fa_size);
	ztest_true(rc == 0, "erasing flash area [%d]\n", rc);

	rc = fs_mount(&littlefs_mnt);
	ztest_true(rc == 0, "mounting littlefs [%d]\n", rc);
}
