/*
 * Copyright (c) 2021 Stephanos Ioannidis <root@stephanos.io>
 * Copyright (C) 2010-2021 ARM Limited or its affiliates. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/zephyr.h>
#include <stdlib.h>
#include <arm_math_f16.h>
#include "../../common/test_common.h"

#include "f16.pat"

#define REL_ERROR_THRESH	(3.0e-3)

/* Note: this source file is only built when CONFIG_CMSIS_DSP_FLOAT16 is enabled */

ZTEST_SUITE(bayes_f16, NULL, NULL, NULL, NULL, NULL);

ZTEST(bayes_f16, test_gaussian_naive_bayes_predict_f16)
{
	arm_gaussian_naive_bayes_instance_f16 inst;

	size_t index;
	const uint16_t pattern_count = in_dims[0];
	const uint16_t class_count = in_dims[1];
	const uint16_t vec_dims = in_dims[2];

	const float16_t *params = (const float16_t *)in_param;
	const float16_t *input = (const float16_t *)in_val;
	float16_t *output_probs_buf, *output_probs;
	uint16_t *output_preds_buf, *output_preds;
	float16_t *temp;

	/* Initialise instance */
	inst.vectorDimension = vec_dims;
	inst.numberOfClasses = class_count;
	inst.theta = params;
	inst.sigma = params + (class_count * vec_dims);
	inst.classPriors = params + (2 * class_count * vec_dims);
	inst.epsilon = params[class_count + (2 * class_count * vec_dims)];

	/* Allocate output buffers */
	output_probs_buf =
		malloc(pattern_count * class_count * sizeof(float16_t));
	zassert_not_null(output_probs_buf, ASSERT_MSG_BUFFER_ALLOC_FAILED);

	output_preds_buf =
		malloc(pattern_count * sizeof(uint16_t));
	zassert_not_null(output_preds_buf, ASSERT_MSG_BUFFER_ALLOC_FAILED);

	output_probs = output_probs_buf;
	output_preds = output_preds_buf;

	temp = malloc(pattern_count * class_count * sizeof(float16_t));
	zassert_not_null(temp, ASSERT_MSG_BUFFER_ALLOC_FAILED);

	/* Enumerate patterns */
	for (index = 0; index < pattern_count; index++) {
		/* Run test function */
		*output_preds =
			arm_gaussian_naive_bayes_predict_f16(
				&inst, input, output_probs, temp);

		/* Increment pointers */
		input += vec_dims;
		output_probs += class_count;
		output_preds++;
	}

	/* Validate output */
	zassert_true(
		test_rel_error_f16(pattern_count, output_probs_buf,
			(float16_t *)ref_prob, REL_ERROR_THRESH),
		ASSERT_MSG_REL_ERROR_LIMIT_EXCEED);

	zassert_true(
		test_equal_q15(pattern_count, output_preds_buf, ref_pred),
		ASSERT_MSG_INCORRECT_COMP_RESULT);

	/* Free output buffers */
	free(output_probs_buf);
	free(output_preds_buf);
}
