/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __OCF_ENV_H__
#define __OCF_ENV_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <linux/limits.h>
#include <linux/stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <semaphore.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>
#include <sys/param.h>
#include <zlib.h>

#include "ocf_env_list.h"
#include "ocf_env_headers.h"

/* linux sector 512-bytes */
#define ENV_SECTOR_SHIFT	9

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef uint64_t sector_t;

#define __packed	__attribute__((packed))
#define __aligned(x)	__attribute__((aligned(x)))

#define likely(cond)       __builtin_expect(!!(cond), 1)
#define unlikely(cond)     __builtin_expect(!!(cond), 0)

#define min(a,b) MIN(a,b)

#define OCF_ALLOCATOR_NAME_MAX 128

/* linux sector 512-bytes */
#define SECTOR_SHIFT	9
#define SECTOR_SIZE	(1<<SECTOR_SHIFT)

#define PAGE_SIZE 4096

/* *** MEMORY MANAGEMENT *** */
#define ENV_MEM_NORMAL	0
#define ENV_MEM_NOIO	0
#define ENV_MEM_ATOMIC	0

static inline void *env_malloc(size_t size, int flags)
{
	return malloc(size);
}

static inline void *env_zalloc(size_t size, int flags)
{
	void *ptr = malloc(size);

	if (ptr)
		memset(ptr, 0, size);

	return ptr;
}

static inline void env_free(const void *ptr)
{
	free((void *)ptr);
}

static inline void *env_vmalloc(size_t size)
{
	return malloc(size);
}

static inline void *env_vzalloc(size_t size)
{
	return env_zalloc(size, 0);
}

static inline void env_vfree(const void *ptr)
{
	free((void *)ptr);
}

static inline uint64_t env_get_free_memory(void)
{
	return sysconf(_SC_PAGESIZE) * sysconf(_SC_AVPHYS_PAGES);
}

/* *** ALLOCATOR *** */

typedef struct _env_allocator env_allocator;

env_allocator *env_allocator_create(uint32_t size, const char *fmt_name, ...);

void env_allocator_destroy(env_allocator *allocator);

void *env_allocator_new(env_allocator *allocator);

void env_allocator_del(env_allocator *allocator, void *item);

uint32_t env_allocator_item_count(env_allocator *allocator);

/* *** MUTEX *** */

typedef struct {
	pthread_mutex_t m;
} env_mutex;

#define env_cond_resched()      ({})

static inline int env_mutex_init(env_mutex *mutex)
{
	if(pthread_mutex_init(&mutex->m, NULL))
		return 1;

	return 0;
}

//TODO: should probably return int
static inline void env_mutex_lock(env_mutex *mutex)
{
	pthread_mutex_lock(&mutex->m);
}

static inline int env_mutex_lock_interruptible(env_mutex *mutex)
{
	env_mutex_lock(mutex);
	return 0;
}

static inline int env_mutex_trylock(env_mutex *mutex)
{
	if (pthread_mutex_trylock(&mutex->m) == 0)
		return 1;
	return 0;
}

//TODO: should probably return int
static inline void env_mutex_unlock(env_mutex *mutex)
{
	pthread_mutex_unlock(&mutex->m);
}

static inline int env_mutex_is_locked(env_mutex *mutex)
{
	if (env_mutex_trylock(mutex)) {
		env_mutex_unlock(mutex);
		return 1;
	}

	return 0;
}

/* *** RECURSIVE MUTEX *** */

typedef env_mutex env_rmutex;

static inline int env_rmutex_init(env_rmutex *rmutex)
{
	env_mutex_init(rmutex);
	return 0;
}

//TODO: should probably return int
static inline void env_rmutex_lock(env_rmutex *rmutex)
{
	env_mutex_lock(rmutex);
}

static inline int env_rmutex_lock_interruptible(env_rmutex *rmutex)
{
	return env_mutex_lock_interruptible(rmutex);
}

static inline int env_rmutex_trylock(env_rmutex *rmutex)
{
	return env_mutex_trylock(rmutex);
}

//TODO: should probably return int
static inline void env_rmutex_unlock(env_rmutex *rmutex)
{
	env_mutex_unlock(rmutex);
}

static inline int env_rmutex_is_locked(env_rmutex *rmutex)
{
	return env_mutex_is_locked(rmutex);
}

/* *** RW SEMAPHORE *** */
typedef struct {
	 pthread_rwlock_t lock;
} env_rwsem;

//TODO: should probably return int
static inline int env_rwsem_init(env_rwsem *s)
{
	pthread_rwlock_init(&s->lock, NULL);
	return 0;
}

//TODO: should probably return int
static inline void env_rwsem_up_read(env_rwsem *s)
{
	pthread_rwlock_unlock(&s->lock);
}

//TODO: should probably return int
static inline void env_rwsem_down_read(env_rwsem *s)
{
	pthread_rwlock_rdlock(&s->lock);
}

static inline int env_rwsem_down_read_trylock(env_rwsem *s)
{
	int result = pthread_rwlock_tryrdlock(&s->lock);

	if (result == 0)
		return 1;
	else
		return 0;
}

//TODO: should probably return int
static inline void env_rwsem_up_write(env_rwsem *s)
{
	pthread_rwlock_unlock(&s->lock);
}

//TODO: should probably return int
static inline void env_rwsem_down_write(env_rwsem *s)
{
	pthread_rwlock_wrlock(&s->lock);
}

static inline int env_rwsem_down_write_trylock(env_rwsem *s)
{
	int result = pthread_rwlock_trywrlock(&s->lock);

	if (result == 0)
		return 1;
	else
		return 0;
}

static inline int env_rwsem_is_locked(env_rwsem *s)
{
	if (env_rwsem_down_read_trylock(s)) {
		env_rwsem_up_read(s);
		return 0;
	}

	return 1;
}

static inline int env_rwsem_down_write_interruptible(env_rwsem *s)
{
	env_rwsem_down_write(s);
	return 0;
}

static inline int env_rwsem_down_read_interruptible(env_rwsem *s)
{
	env_rwsem_down_read(s);
	return 0;
}

/* *** COMPLETION *** */
struct completion {
	sem_t sem;
};

typedef struct completion env_completion;

//TODO: should probably return int
static inline void env_completion_init(env_completion *completion)
{
	sem_init(&completion->sem, 0, 0);
}

//TODO: should probably return int
static inline void env_completion_wait(env_completion *completion)
{
	sem_wait(&completion->sem);
}

//TODO: should probably return int
static inline void env_completion_complete(env_completion *completion)
{
	sem_post(&completion->sem);
}

//TODO: should probably return int
static inline void env_completion_complete_and_exit(
		env_completion *completion, int ret)
{
	env_completion_complete(completion); /* TODO */
}

/* *** ATOMIC VARIABLES *** */

typedef struct {
	volatile int counter;
} env_atomic;

typedef struct {
	volatile long counter;
} env_atomic64;

static inline int env_atomic_read(const env_atomic *a)
{
	return a->counter; /* TODO */
}

static inline void env_atomic_set(env_atomic *a, int i)
{
	a->counter = i; /* TODO */
}

static inline void env_atomic_add(int i, env_atomic *a)
{
	__sync_add_and_fetch(&a->counter, i);
}

static inline void env_atomic_sub(int i, env_atomic *a)
{
	__sync_sub_and_fetch(&a->counter, i);
}

static inline bool env_atomic_sub_and_test(int i, env_atomic *a)
{
	return __sync_sub_and_fetch(&a->counter, i) == 0;
}

static inline void env_atomic_inc(env_atomic *a)
{
	env_atomic_add(1, a);
}

static inline void env_atomic_dec(env_atomic *a)
{
	env_atomic_sub(1, a);
}

static inline bool env_atomic_dec_and_test(env_atomic *a)
{
	return __sync_sub_and_fetch(&a->counter, 1) == 0;
}

static inline bool env_atomic_inc_and_test(env_atomic *a)
{
	return __sync_add_and_fetch(&a->counter, 1) == 0;
}

static inline int env_atomic_add_return(int i, env_atomic *a)
{
	return __sync_add_and_fetch(&a->counter, i);
}

static inline int env_atomic_sub_return(int i, env_atomic *a)
{
	return __sync_sub_and_fetch(&a->counter, i);
}

static inline int env_atomic_inc_return(env_atomic *a)
{
	return env_atomic_add_return(1, a);
}

static inline int env_atomic_dec_return(env_atomic *a)
{
	return env_atomic_sub_return(1, a);
}

static inline int env_atomic_cmpxchg(env_atomic *a, int old, int new_value)
{
	return __sync_val_compare_and_swap(&a->counter, old, new_value);
}

static inline int env_atomic_add_unless(env_atomic *a, int i, int u)
{
	int c, old;
	c = env_atomic_read(a);
	for (;;) {
		if (unlikely(c == (u)))
			break;
		old = env_atomic_cmpxchg((a), c, c + (i));
		if (likely(old == c))
			break;
		c = old;
	}
	return c != (u);
}

static inline long env_atomic64_read(const env_atomic64 *a)
{
	return a->counter; /* TODO */
}

static inline void env_atomic64_set(env_atomic64 *a, long i)
{
	a->counter = i; /* TODO */
}

static inline void env_atomic64_add(long i, env_atomic64 *a)
{
	__sync_add_and_fetch(&a->counter, i);
}

static inline void env_atomic64_sub(long i, env_atomic64 *a)
{
	__sync_sub_and_fetch(&a->counter, i);
}

static inline void env_atomic64_inc(env_atomic64 *a)
{
	env_atomic64_add(1, a);
}

static inline void env_atomic64_dec(env_atomic64 *a)
{
	env_atomic64_sub(1, a);
}

static inline long env_atomic64_cmpxchg(env_atomic64 *a, long old, long new)
{
	return __sync_val_compare_and_swap(&a->counter, old, new);
}

/* *** SPIN LOCKS *** */

typedef struct {
	pthread_spinlock_t lock;
} env_spinlock;

//TODO: should probably return int
static inline void env_spinlock_init(env_spinlock *l)
{
	pthread_spin_init(&l->lock, 0);
}

static inline void env_spinlock_lock(env_spinlock *l)
{
	pthread_spin_lock(&l->lock);
}

static inline void env_spinlock_unlock(env_spinlock *l)
{
	pthread_spin_unlock(&l->lock);
}

static inline void env_spinlock_lock_irq(env_spinlock *l)
{
	env_spinlock_lock(l);
}

static inline void env_spinlock_unlock_irq(env_spinlock *l)
{
	env_spinlock_unlock(l);
}

#define env_spinlock_lock_irqsave(l, flags) \
		env_spinlock_lock(l)

#define env_spinlock_unlock_irqrestore(l, flags) \
		env_spinlock_unlock(l)

/* *** RW LOCKS *** */

typedef struct {
	pthread_rwlock_t lock;
} env_rwlock;

static inline void env_rwlock_init(env_rwlock *l)
{
	pthread_rwlock_init(&l->lock, NULL);
}

static inline void env_rwlock_read_lock(env_rwlock *l)
{
	pthread_rwlock_rdlock(&l->lock);
}

static inline void env_rwlock_read_unlock(env_rwlock *l)
{
	pthread_rwlock_unlock(&l->lock);
}

static inline void env_rwlock_write_lock(env_rwlock *l)
{
	pthread_rwlock_wrlock(&l->lock);
}

static inline void env_rwlock_write_unlock(env_rwlock *l)
{
	pthread_rwlock_unlock(&l->lock);
}

/* *** WAITQUEUE *** */

typedef struct {
	sem_t sem;
} env_waitqueue;

static inline void env_waitqueue_init(env_waitqueue *w)
{
	sem_init(&w->sem, 0, 0);
}

static inline void env_waitqueue_wake_up(env_waitqueue *w)
{
	sem_post(&w->sem);
}

#define env_waitqueue_wait(w, condition)	\
({						\
	int __ret = 0;				\
	if (!(condition))			\
		sem_wait(&w.sem);		\
	__ret = __ret;				\
})

/* *** BIT OPERATIONS *** */

static inline void env_bit_set(int nr, volatile void *addr)
{
	char *byte = (char *)addr + (nr >> 3);
	char mask = 1 << (nr & 7);

	__sync_or_and_fetch(byte, mask);
}

static inline void env_bit_clear(int nr, volatile void *addr)
{
	char *byte = (char *)addr + (nr >> 3);
	char mask = 1 << (nr & 7);

	mask = ~mask;
	__sync_and_and_fetch(byte, mask);
}

static inline bool env_bit_test(int nr, const volatile unsigned long *addr)
{
	const char *byte = (char *)addr + (nr >> 3);
	char mask = 1 << (nr & 7);

	return !!(*byte & mask);
}

/* *** SCHEDULING *** */
static inline void env_schedule(void)
{
	sched_yield();
}

static inline int env_in_interrupt(void)
{
	return 0;
}

/* TODO(mwysocza): Is this really needed? */
static inline void env_touch_lockup_watchdog(void)
{
}

/* TODO(mwysocza): Jiffies replacament. Need smart implementation */
static inline uint64_t env_get_tick_count(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static inline uint64_t env_ticks_to_msecs(uint64_t j)
{
	return j;
}

static inline uint64_t env_ticks_to_secs(uint64_t j)
{
	return j / 1000;
}

static inline uint64_t env_secs_to_ticks(uint64_t j)
{
	return j * 1000;
}

/* *** SORTING *** */

static inline void env_sort(void *base, size_t num, size_t size,
		int (*cmp_fn)(const void *, const void *),
		void (*swap_fn)(void *, void *, int size))
{
	qsort(base, num, size, cmp_fn);
}

/* *** STRING OPERATIONS *** */
#define env_memset(dest, dmax, val) ({ \
		memset(dest, val, dmax); \
		0; \
	})
#define env_memcpy(dest, dmax, src, slen) ({ \
		memcpy(dest, src, min(dmax, slen)); \
		0; \
	})
#define env_memcmp(s1, s1max, s2, s2max, diff) ({ \
		*diff = memcmp(s1, s2, min(s1max, s2max)); \
		0; \
	})
#define env_strdup strndup
#define env_strnlen(s, smax) strnlen(s, smax)
#define env_strncmp strncmp
#define env_strncpy(dest, dmax, src, slen) ({ \
		strncpy(dest, src, min(dmax, slen)); \
		0; \
	})
/* *** DEBUGING *** */

/* TODO(mwysocza): Check if this implementation is good enough */
#define ENV_WARN(cond, fmt...)		printf(fmt)
#define ENV_WARN_ON(cond)		;
#define ENV_WARN_ONCE(cond, fmt...)	ENV_WARN(cond, fmt)

#define ENV_BUG()			assert(0)
#define ENV_BUG_ON(cond)		assert(!(cond))

#define container_of(ptr, type, member) ({          \
	const typeof(((type *)0)->member)*__mptr = (ptr);    \
	(type *)((char *)__mptr - offsetof(type, member)); })

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

static inline void env_msleep(uint64_t n)
{
	usleep(n * 1000);
}

struct env_timeval {
	uint64_t sec, usec;
};

static inline void env_gettimeofday(struct env_timeval *tv)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	tv->sec = t.tv_sec;
	tv->usec = t.tv_usec;
}

uint32_t env_crc32(uint32_t crc, uint8_t const *data, size_t len);

#define ENV_PRIu64 "lu"

#endif /* __OCF_ENV_H__ */
