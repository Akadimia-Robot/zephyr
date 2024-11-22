#!/bin/env bash
# Copyright 2023 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

source ${ZEPHYR_BASE}/tests/bsim/sh_common.source

simulation_id="ead_sample"
verbosity_level=2

cd ${BSIM_OUT_PATH}/bin

if [ "${BOARD_TS}" = "nrf52_bsim" ]; then
  BOARD_TS="${BOARD_TS}_native"
fi

Execute ./bs_${BOARD_TS}_tests_bsim_bluetooth_host_adv_encrypted_ead_sample_prj_conf \
  -v=${verbosity_level} -s=${simulation_id} -d=0 -testid=central -RealEncryption=1

Execute ./bs_${BOARD_TS}_tests_bsim_bluetooth_host_adv_encrypted_ead_sample_prj_conf \
  -v=${verbosity_level} -s=${simulation_id} -d=1 -testid=peripheral -RealEncryption=1

Execute ./bs_2G4_phy_v1 -v=${verbosity_level} -s=${simulation_id} \
  -D=2 -sim_length=60e6 $@

wait_for_background_jobs
