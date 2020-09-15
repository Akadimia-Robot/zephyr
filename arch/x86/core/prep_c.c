/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <kernel_internal.h>
#include <arch/x86/acpi.h>
#include <arch/x86/multiboot.h>
#include <arch/x86/efi.h>
#include <x86_mmu.h>

extern FUNC_NORETURN void z_cstart(void);
extern void x86_64_irq_init(void);

#if !defined(CONFIG_X86_64)
x86_boot_arg_t *x86_cpu_boot_arg;
#endif

/* Early global initialization functions, C domain. This runs only on the first
 * CPU for SMP systems.
 */
FUNC_NORETURN void z_x86_prep_c(void *arg)
{
	x86_boot_arg_t *cpu_arg = arg;

	_kernel.cpus[0].nested = 0;

#ifdef CONFIG_X86_VERY_EARLY_CONSOLE
	z_x86_early_serial_init();
#endif

#ifdef CONFIG_X86_64
	x86_64_irq_init();
#endif

	if (IS_ENABLED(CONFIG_MULTIBOOT_INFO) &&
	    cpu_arg->boot_type == MULTIBOOT_BOOT_TYPE) {
		z_multiboot_init((struct multiboot_info *)cpu_arg->arg);
	} else if (IS_ENABLED(CONFIG_X86_EFI_SYSTEM_TABLE) &&
		   cpu_arg->boot_type == EFI_BOOT_TYPE) {
		efi_init((struct efi_system_table *)cpu_arg->arg);
	} else {
		ARG_UNUSED(cpu_arg);
	}

#if CONFIG_X86_STACK_PROTECTION
	for (int i = 0; i < CONFIG_MP_NUM_CPUS; i++) {
		z_x86_set_stack_guard(z_interrupt_stacks[i]);
	}
#endif

#if defined(CONFIG_SMP)
	z_x86_ipi_setup();
#endif

	z_cstart();
}
