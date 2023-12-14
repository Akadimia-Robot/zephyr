/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_POSIX_TYPES_H_
#define ZEPHYR_INCLUDE_POSIX_TYPES_H_

#if !(defined(CONFIG_ARCH_POSIX) && defined(CONFIG_EXTERNAL_LIBC))
#include <sys/types.h>
#endif

#ifdef CONFIG_NEWLIB_LIBC
#include <sys/_pthreadtypes.h>
#endif

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __useconds_t_defined
typedef unsigned long useconds_t;
#endif

/* time related attributes */
#if !defined(CONFIG_NEWLIB_LIBC) && !defined(CONFIG_ARCMWDT_LIBC)
#ifndef __clockid_t_defined
typedef uint32_t clockid_t;
#endif
#endif /* !CONFIG_NEWLIB_LIBC && !CONFIG_ARCMWDT_LIBC */
#ifndef __timer_t_defined
typedef unsigned long timer_t;
#endif

/* Thread attributes */
struct pthread_attr {
	void *stack;
	uint16_t stacksize;
	int8_t priority;
	uint8_t schedpolicy: 2;
	bool initialized: 1;
	bool cancelstate: 1;
	bool detachstate: 1;
};
#if defined(CONFIG_MINIMAL_LIBC) || defined(CONFIG_PICOLIBC) || defined(CONFIG_ARMCLANG_STD_LIBC) \
	|| defined(CONFIG_ARCMWDT_LIBC)
typedef struct pthread_attr pthread_attr_t;
#endif

BUILD_ASSERT(sizeof(pthread_attr_t) >= sizeof(struct pthread_attr));

typedef uint32_t pthread_t;
typedef uint32_t pthread_spinlock_t;

/* Semaphore */
typedef struct k_sem sem_t;

/* Mutex */
typedef uint32_t pthread_mutex_t;

struct pthread_mutexattr {
	int type;
};
#if defined(CONFIG_MINIMAL_LIBC) || defined(CONFIG_PICOLIBC) || defined(CONFIG_ARMCLANG_STD_LIBC) \
	|| defined(CONFIG_ARCMWDT_LIBC)
typedef struct pthread_mutexattr pthread_mutexattr_t;
#endif
BUILD_ASSERT(sizeof(pthread_mutexattr_t) >= sizeof(struct pthread_mutexattr));

/* Condition variables */
typedef uint32_t pthread_cond_t;

struct pthread_condattr {
	clockid_t clock;
};

#if defined(CONFIG_MINIMAL_LIBC) || defined(CONFIG_PICOLIBC) || defined(CONFIG_ARMCLANG_STD_LIBC) \
	|| defined(CONFIG_ARCMWDT_LIBC)
typedef struct pthread_condattr pthread_condattr_t;
#endif
BUILD_ASSERT(sizeof(pthread_condattr_t) >= sizeof(struct pthread_condattr));

/* Barrier */
typedef uint32_t pthread_barrier_t;

typedef struct pthread_barrierattr {
	int pshared;
} pthread_barrierattr_t;

typedef uint32_t pthread_rwlockattr_t;

typedef struct pthread_rwlock_obj {
	struct k_sem rd_sem;
	struct k_sem wr_sem;
	struct k_sem reader_active;/* blocks WR till reader has acquired lock */
	int32_t status;
	k_tid_t wr_owner;
} pthread_rwlock_t;

#ifdef __cplusplus
}
#endif

#endif	/* ZEPHYR_INCLUDE_POSIX_TYPES_H_ */
