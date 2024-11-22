#!/usr/bin/env bash
# Copyright 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

# Run bluetooth/host tests that support nrf5340bsim/nrf5340/cpunet platform

set -ue

: "${ZEPHYR_BASE:?ZEPHYR_BASE must be set to point to the zephyr root directory}"

board=nrf5340bsim/nrf5340/cpunet
test_spec=testlist.json

west twister -T ${ZEPHYR_BASE}/tests/bsim/bluetooth/host/ -p ${board} -E ${test_spec}

parse_cmd="['${ZEPHYR_BASE}/' + it['path'] for it in json.load(open('${test_spec}'))['testsuites']]"
search_path=`python3 -c "import sys, json; print(' '.join(set(${parse_cmd})))"`

rm -f ${test_spec}

BOARD=${board} \
RESULTS_FILE=${ZEPHYR_BASE}/bsim_out/bsim_results.bt.host.53_cpunet.xml \
SEARCH_PATH=${search_path} \
${ZEPHYR_BASE}/tests/bsim/run_parallel.sh
