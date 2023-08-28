/*
 * Copyright (c) 2015 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_INIT_H_
#define ZEPHYR_INCLUDE_INIT_H_

#include <stdint.h>
#include <stddef.h>

#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup sys_init System Initialization
 * @ingroup os_services
 *
 * Zephyr offers an infrastructure to call initialization code before `main`.
 * Such initialization calls can be registered using SYS_INIT() or
 * SYS_INIT_NAMED() macros. By using a combination of initialization levels and
 * priorities init sequence can be adjusted as needed. The available
 * initialization levels are described, in order, below:
 *
 * - `EARLY`: Used very early in the boot process, right after entering the C
 *   domain (``z_cstart()``). This can be used in architectures and SoCs that
 *   extend or implement architecture code and use drivers or system services
 *   that have to be initialized before the Kernel calls any architecture
 *   specific initialization code.
 * - `PRE_KERNEL_1`: Executed in Kernel's initialization context, which uses
 *   the interrupt stack. At this point Kernel services are not yet available.
 * - `PRE_KERNEL_2`: Same as `PRE_KERNEL_1`.
 * - `POST_KERNEL`: Executed after Kernel is alive. From this point on, Kernel
 *   primitives can be used.
 * - `APPLICATION`: Executed just before application code (`main`).
 * - `SMP`: Only available if @kconfig{CONFIG_SMP} is enabled, specific for
 *   SMP.
 *
 * Initialization priority can take a value in the range of 0 to 99.
 * @{
 */

/** System initialization function. */
typedef int (*sys_init_fn_t)(void);

/** @cond INTERNAL_HIDDEN */

/**
 * @brief Obtain init entry name.
 *
 * @param init_id Init entry unique identifier.
 */
#define Z_INIT_ENTRY_NAME(init_id) _CONCAT(__init_, init_id)

/**
 * @brief Init entry section.
 *
 * Each init entry is placed in a section with a name crafted so that it allows
 * linker scripts to sort them according to the specified
 * level/priority/sub-priority.
 */
#define Z_INIT_ENTRY_SECTION(level, prio)                                      \
	__attribute__(                                                         \
		(__section__(".z_init_" #level STRINGIFY(prio)"_")))

/** @endcond */

/**
 * @brief Register an initialization function.
 *
 * The function will be called during system initialization according to the
 * given level and priority.
 *
 * @param init_fn Initialization function.
 * @param level Initialization level. Allowed tokens: `EARLY`, `PRE_KERNEL_1`,
 * `PRE_KERNEL_2`, `POST_KERNEL`, `APPLICATION` and `SMP` if
 * @kconfig{CONFIG_SMP} is enabled.
 * @param prio Initialization priority within @p _level. Note that it must be a
 * decimal integer literal without leading zeroes or sign (e.g. `32`), or an
 * equivalent symbolic name (e.g. `#define MY_INIT_PRIO 32`); symbolic
 * expressions are **not** permitted (e.g.
 * `CONFIG_KERNEL_INIT_PRIORITY_DEFAULT + 5`).
 */
#define SYS_INIT(init_fn, level, prio)                                         \
	SYS_INIT_NAMED(init_fn, init_fn, level, prio)

/**
 * @brief Register an initialization function (named).
 *
 * @note This macro can be used for cases where the multiple init calls use the
 * same init function.
 *
 * @param name Unique name for SYS_INIT entry.
 * @param init_fn See SYS_INIT().
 * @param level See SYS_INIT().
 * @param prio See SYS_INIT().
 *
 * @see SYS_INIT()
 */
#define SYS_INIT_NAMED(name, init_fn, level, prio)                             \
	static const Z_DECL_ALIGN(sys_init_fn_t)                               \
		Z_INIT_ENTRY_SECTION(level, prio) __used __noasan              \
		Z_INIT_ENTRY_NAME(name) = init_fn

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_INIT_H_ */
