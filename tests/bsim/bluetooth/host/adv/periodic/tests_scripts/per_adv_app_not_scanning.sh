#!/usr/bin/env bash
# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

# Periodic advertising sync test where the host starts scanning
# automatically because the application didn't start it.

source ${ZEPHYR_BASE}/tests/bsim/sh_common.source

simulation_id="per_adv_not_scanning"
verbosity_level=2

cd ${BSIM_OUT_PATH}/bin

if [ "${BOARD_TS}" = "nrf52_bsim" ]; then
  BOARD_TS="${BOARD_TS}_native"
fi

Execute ./bs_${BOARD_TS}_tests_bsim_bluetooth_host_adv_periodic_prj_conf \
  -v=${verbosity_level} -s=${simulation_id} -d=0 -RealEncryption=0 \
  -testid=per_adv_advertiser -rs=23

Execute ./bs_${BOARD_TS}_tests_bsim_bluetooth_host_adv_periodic_prj_conf \
  -v=${verbosity_level} -s=${simulation_id} -d=1 -RealEncryption=0 \
  -testid=per_adv_sync_app_not_scanning -rs=6

Execute ./bs_2G4_phy_v1 -v=${verbosity_level} -s=${simulation_id} \
  -D=2 -sim_length=20e6 $@

wait_for_background_jobs
