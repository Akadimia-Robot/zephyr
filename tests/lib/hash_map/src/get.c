/*
 * Copyright (c) 2022 Meta
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/sys/hash_map.h>

#include "_main.h"

#if defined(CONFIG_TEST_USE_CUSTOM_EQ_FUNC)
#define KEY_1   "first_key"
#define VALUE_1 1
#define KEY_2   "second_key"
#define VALUE_2 2
#endif

ZTEST(hash_map, test_get_true)
{
	int ret;
	uint64_t value = 0x42;

	zassert_true(sys_hashmap_is_empty(&map));

#if !defined(CONFIG_TEST_USE_CUSTOM_EQ_FUNC)
	zassert_equal(1, sys_hashmap_insert(&map, 0, 0, NULL, NULL));
	zassert_true(sys_hashmap_get(&map, 0, NULL));
	zassert_true(sys_hashmap_get(&map, 0, &value));
	zassert_equal(0, value);
#else
	zassert_equal(1, string_map_insert(&map, KEY_1, VALUE_1, NULL, NULL));
	zassert_true(string_map_get(&map, KEY_1, NULL));
	zassert_true(string_map_get(&map, KEY_1, &value));
	zassert_equal(VALUE_1, value);
#endif

	for (size_t i = 1; i < MANY; ++i) {
#if !defined(CONFIG_TEST_USE_CUSTOM_EQ_FUNC)
		ret = sys_hashmap_insert(&map, i, i, NULL, NULL);
		zassert_equal(1, ret, "failed to insert (%zu, %zu): %d", i, i, ret);
#else
		char *index_str = alloc_string_from_index(i);

		ret = string_map_insert(&map, index_str, i, NULL, NULL);
		zassert_equal(1, ret, "failed to insert (%s, %zu): %d", index_str, i, ret);
#endif
	}

	for (size_t i = 0; i < MANY; ++i) {
#if !defined(CONFIG_TEST_USE_CUSTOM_EQ_FUNC)
		zassert_true(sys_hashmap_get(&map, i, NULL));
#else
		char *index_str = alloc_string_from_index(i);

		zassert_true(string_map_get(&map, index_str, NULL));
		free(index_str);
#endif
	}

#if defined(CONFIG_TEST_USE_CUSTOM_EQ_FUNC)
	/* free the hashmap keys correctly before exiting */
	sys_hashmap_clear(&map, string_map_free_callback, NULL);
#endif
}

ZTEST(hash_map, test_get_false)
{
	int ret;
	uint64_t value = 0x42;

	zassert_true(sys_hashmap_is_empty(&map));

#if !defined(CONFIG_TEST_USE_CUSTOM_EQ_FUNC)
	zassert_false(sys_hashmap_get(&map, 73, &value));
	zassert_equal(value, 0x42);
#else
	zassert_false(string_map_get(&map, KEY_2, &value));
	zassert_equal(value, 0x42);
#endif

	for (size_t i = 0; i < MANY; ++i) {
#if !defined(CONFIG_TEST_USE_CUSTOM_EQ_FUNC)
		ret = sys_hashmap_insert(&map, i, i, NULL, NULL);
		zassert_equal(1, ret, "failed to insert (%zu, %zu): %d", i, i, ret);
#else
		char *index_str = alloc_string_from_index(i);

		ret = string_map_insert(&map, index_str, i, NULL, NULL);
		zassert_equal(1, ret, "failed to insert (%s, %zu): %d", index_str, i, ret);
#endif
	}

#if !defined(CONFIG_TEST_USE_CUSTOM_EQ_FUNC)
	zassert_false(sys_hashmap_get(&map, 0x4242424242424242ULL, NULL));
#else
	zassert_false(string_map_get(&map, KEY_1, NULL));
	/* free the hashmap keys correctly before exiting */
	sys_hashmap_clear(&map, string_map_free_callback, NULL);
#endif
}
