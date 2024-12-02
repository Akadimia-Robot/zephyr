#!/usr/bin/env python3
#
# Copyright 2024 NXP
#
# SPDX-License-Identifier: Apache-2.0
#
# Based on gen_device_deps.py, which is
#
# Copyright (c) 2017 Intel Corporation
# Copyright (c) 2020 Nordic Semiconductor NA
"""Translate clock dependency ordinals into clock objects usable by the
application.
This script will run on the link stage zephyr executable, and identify
the clock dependency arrays generated by the first build pass. It will
then create a source file with strong symbol definitions to override the
existing symbol definitions (which must be weak) and replace the
clock dependency ordinals in the array with clock structure references.
"""

import argparse
import os
import pickle
import sys

from elf_parser import ZephyrElf

# This is needed to load edt.pickle files.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "dts", "python-devicetree", "src"))


def parse_args():
    global args

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        allow_abbrev=False,
    )

    parser.add_argument("-k", "--kernel", required=True, help="Input zephyr ELF binary")
    parser.add_argument("-o", "--output-source", required=True, help="Output source file")
    parser.add_argument(
        "-z",
        "--zephyr-base",
        help="Path to current Zephyr base. If this argument \
                        is not provided the environment will be checked for \
                        the ZEPHYR_BASE environment variable.",
    )
    parser.add_argument(
        "-s",
        "--start-symbol",
        required=True,
        help="Symbol name of the section which contains the \
                        devices. The symbol name must point to the first \
                        device in that section.",
    )

    args = parser.parse_args()

    ZEPHYR_BASE = args.zephyr_base or os.getenv("ZEPHYR_BASE")

    if ZEPHYR_BASE is None:
        sys.exit(
            "-z / --zephyr-base not provided. Please provide "
            "--zephyr-base or set ZEPHYR_BASE in environment"
        )

    sys.path.insert(0, os.path.join(ZEPHYR_BASE, "scripts/dts"))


def main():
    parse_args()

    edtser = os.path.join(os.path.split(args.kernel)[0], "edt.pickle")
    with open(edtser, "rb") as f:
        edt = pickle.load(f)

    parsed_elf = ZephyrElf(args.kernel, edt, args.start_symbol)

    with open(args.output_source, "w") as fp:
        fp.write("#include <zephyr/drivers/clock_management/clock.h>\n\n")

        # Iterate through all clock ordinals lists in the system, and
        # for each one define a new array with the clock objects needed
        # for the final application
        for ord_num, child_ords in parsed_elf.clock_ordinal_arrays.items():
            sym_name = f"__clock_children_clk_dts_ord_{ord_num}"
            sym_values = []
            for child_handles in child_ords.handles:
                sym_values.append(f"{child_handles}")
            sym_values.append("CLOCK_LIST_END")
            sym_array = ",\n\t".join(sym_values)
            fp.write(f"const clock_handle_t {sym_name}[] = \n\t{{{sym_array}}};\n\n")


if __name__ == "__main__":
    main()
