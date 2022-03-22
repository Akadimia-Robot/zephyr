/*
 * Copyright © 2021, Keith Packard <keithp@keithp.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/cpu.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <linker/linker-defs.h>
#include <sys/util.h>
#include <sys/time.h>
#include <sys/errno_private.h>
#include <sys/libc-hooks.h>
#include <syscall_handler.h>
#include <app_memory/app_memdomain.h>
#include <init.h>
#include <sys/sem.h>

#define LIBC_BSS	K_APP_BMEM(z_libc_partition)
#define LIBC_DATA	K_APP_DMEM(z_libc_partition)

#if CONFIG_PICOLIBC_ALIGNED_HEAP_SIZE
K_APPMEM_PARTITION_DEFINE(z_malloc_partition);
#define MALLOC_BSS	K_APP_BMEM(z_malloc_partition)

/* Compiler will throw an error if the provided value isn't a power of two */
MALLOC_BSS static unsigned char __aligned(CONFIG_PICOLIBC_ALIGNED_HEAP_SIZE)
	heap_base[CONFIG_PICOLIBC_ALIGNED_HEAP_SIZE];
#define MAX_HEAP_SIZE CONFIG_PICOLIBC_ALIGNED_HEAP_SIZE

#else /* CONFIG_PICOLIBC_ALIGNED_HEAP_SIZE */
/* Heap base and size are determined based on the available unused SRAM,
 * in the interval from a properly aligned address after the linker symbol
 * `_end`, to the end of SRAM
 */
#define USED_RAM_END_ADDR   POINTER_TO_UINT(&_end)

#ifdef Z_MALLOC_PARTITION_EXISTS
/* Need to be able to program a memory protection region from HEAP_BASE
 * to the end of RAM so that user threads can get at it.
 * Implies that the base address needs to be suitably aligned since the
 * bounds have to go in a k_mem_partition.
 */
#ifdef CONFIG_MMU
/* Linker script may already have done this, but just to be safe */
#define HEAP_BASE	ROUND_UP(USED_RAM_END_ADDR, CONFIG_MMU_PAGE_SIZE)
#else /* MPU-based systems */
/* TODO: Need a generic Kconfig for the MPU region granularity */
#if defined(CONFIG_ARM)
#define HEAP_BASE	ROUND_UP(USED_RAM_END_ADDR, \
				 CONFIG_ARM_MPU_REGION_MIN_ALIGN_AND_SIZE)
#elif defined(CONFIG_ARC)
#define HEAP_BASE	ROUND_UP(USED_RAM_END_ADDR, Z_ARC_MPU_ALIGN)
#else
#error "Unsupported platform"
#endif /* CONFIG_<arch> */
#endif /* !CONFIG_MMU */
#else /* !Z_MALLOC_PARTITION_EXISTS */
/* No partition, heap can just start wherever _end is */
#define HEAP_BASE	USED_RAM_END_ADDR
#endif /* Z_MALLOC_PARTITION_EXISTS */

#ifdef CONFIG_XTENSA
extern void *_heap_sentry;
#define MAX_HEAP_SIZE  (POINTER_TO_UINT(&_heap_sentry) - HEAP_BASE)
#else
#define MAX_HEAP_SIZE	(KB(CONFIG_SRAM_SIZE) - \
			 (HEAP_BASE - CONFIG_SRAM_BASE_ADDRESS))
#endif

#if Z_MALLOC_PARTITION_EXISTS
struct k_mem_partition z_malloc_partition;

static int malloc_prepare(const struct device *unused)
{
	ARG_UNUSED(unused);

	z_malloc_partition.start = HEAP_BASE;
	z_malloc_partition.size = MAX_HEAP_SIZE;
	z_malloc_partition.attr = K_MEM_PARTITION_P_RW_U_RW;
	return 0;
}

SYS_INIT(malloc_prepare, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#endif /* CONFIG_USERSPACE */
#endif /* CONFIG_PICOLIBC_ALIGNED_HEAP_SIZE */

LIBC_BSS static unsigned int heap_sz;

static int (*put_hook)(int);

static int picolibc_put(char a, FILE *out)
{
	(*put_hook)(a);
	return 0;
}

static FILE __stdio = FDEV_SETUP_STREAM(picolibc_put, NULL, NULL, 0);

#ifdef __strong_reference
#define STDIO_ALIAS(x) __strong_reference(stdin, x);
#else
#define STDIO_ALIAS(x) FILE *const x = &__stdio;
#endif

FILE *const stdin = &__stdio;
STDIO_ALIAS(stdout);
STDIO_ALIAS(stderr);

void __stdout_hook_install(int (*hook)(int))
{
	put_hook = hook;
	__stdio.flags |= _FDEV_SETUP_WRITE;
}

void __stdin_hook_install(unsigned char (*hook)(void))
{
	__stdio.get = (int (*)(FILE *)) hook;
	__stdio.flags |= _FDEV_SETUP_READ;
}

int z_impl_zephyr_read_stdin(char *buf, int nbytes)
{
	int i = 0;

	for (i = 0; i < nbytes; i++) {
		*(buf + i) = getchar();
		if ((*(buf + i) == '\n') || (*(buf + i) == '\r')) {
			i++;
			break;
		}
	}
	return i;
}

int z_impl_zephyr_write_stdout(const void *buffer, int nbytes)
{
	const char *buf = buffer;
	int i;

	for (i = 0; i < nbytes; i++) {
		if (*(buf + i) == '\n') {
			putchar('\r');
		}
		putchar(*(buf + i));
	}
	return nbytes;
}

static FILE __console = FDEV_SETUP_STREAM(NULL, NULL, NULL, 0);

void __printk_hook_install(int (*fn)(int))
{
	__console.put = (int(*)(char, FILE *)) fn;
	__console.flags |= _FDEV_SETUP_WRITE;
}

void vprintk(const char *fmt, va_list ap)
{
	vfprintf(&__console, fmt, ap);
}

void printk(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(&__console, fmt, ap);
	va_end(ap);
}

#include <sys/cbprintf.h>

struct cb_bits {
	FILE f;
	cbprintf_cb out;
	void	*ctx;
};

static int cbputc(char c, FILE *_s)
{
	struct cb_bits *s = (struct cb_bits *) _s;

	(*s->out) (c, s->ctx);
	return 0;
}

int cbvprintf(cbprintf_cb out, void *ctx, const char *fp, va_list ap)
{
	struct cb_bits	s = {
		.f = FDEV_SETUP_STREAM(cbputc, NULL, NULL, _FDEV_SETUP_WRITE),
		.out = out,
		.ctx = ctx,
	};
	return vfprintf(&s.f, fp, ap);
}

#ifndef CONFIG_POSIX_API
int _read(int fd, char *buf, int nbytes)
{
	ARG_UNUSED(fd);

	return z_impl_zephyr_read_stdin(buf, nbytes);
}
__weak FUNC_ALIAS(_read, read, int);

int _write(int fd, const void *buf, int nbytes)
{
	ARG_UNUSED(fd);

	return z_impl_zephyr_write_stdout(buf, nbytes);
}
__weak FUNC_ALIAS(_write, write, int);

int _open(const char *name, int mode)
{
	return -1;
}
__weak FUNC_ALIAS(_open, open, int);

int _close(int file)
{
	return -1;
}
__weak FUNC_ALIAS(_close, close, int);

int _lseek(int file, int ptr, int dir)
{
	return 0;
}
__weak FUNC_ALIAS(_lseek, lseek, int);
#else
extern ssize_t write(int file, const char *buffer, size_t count);
#define _write	write
#endif

int _isatty(int file)
{
	return 1;
}
__weak FUNC_ALIAS(_isatty, isatty, int);

int _kill(int i, int j)
{
	return 0;
}
__weak FUNC_ALIAS(_kill, kill, int);

int _getpid(void)
{
	return 0;
}
__weak FUNC_ALIAS(_getpid, getpid, int);

int _fstat(int file, struct stat *st)
{
	st->st_mode = S_IFCHR;
	return 0;
}
__weak FUNC_ALIAS(_fstat, fstat, int);

__weak void _exit(int status)
{
	_write(1, "exit\n", 5);
	while (1) {
		;
	}
}

static LIBC_DATA SYS_SEM_DEFINE(heap_sem, 1, 1);

void *_sbrk(int count)
{
	void *ret, *ptr;

	sys_sem_take(&heap_sem, K_FOREVER);

#if CONFIG_PICOLIBC_ALIGNED_HEAP_SIZE
	ptr = heap_base + heap_sz;
#else
	ptr = ((char *)HEAP_BASE) + heap_sz;
#endif

	if ((heap_sz + count) < MAX_HEAP_SIZE) {
		heap_sz += count;
		ret = ptr;
	} else {
		ret = (void *)-1;
	}

	/* coverity[CHECKED_RETURN] */
	sys_sem_give(&heap_sem);

	return ret;
}
__weak FUNC_ALIAS(_sbrk, sbrk, void *);

/* This function gets called if static buffer overflow detection is enabled on
 * stdlib side (Picolibc here), in case such an overflow is detected. Picolibc
 * provides an implementation not suitable for us, so we override it here.
 */
__weak FUNC_NORETURN void __chk_fail(void)
{
	static const char chk_fail_msg[] = "* buffer overflow detected *\n";

	_write(2, chk_fail_msg, sizeof(chk_fail_msg) - 1);
	k_oops();
	CODE_UNREACHABLE;
}

int _gettimeofday(struct timeval *__tp, void *__tzp)
{
	ARG_UNUSED(__tp);
	ARG_UNUSED(__tzp);

	return -1;
}
