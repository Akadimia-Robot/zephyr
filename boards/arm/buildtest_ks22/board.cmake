# SPDX-License-Identifier: Apache-2.0

board_runner_args(jlink "--device=MKS22FN256xxx12" "--speed=4000")

include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
