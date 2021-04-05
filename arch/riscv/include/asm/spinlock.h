/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2015 Regents of the University of California
 * Copyright (C) 2017 SiFive
 * Copyright (c) 2021 Christoph Müllner <cmuellner@linux.com>
 */

#ifndef _ASM_RISCV_SPINLOCK_H
#define _ASM_RISCV_SPINLOCK_H

#include <linux/kernel.h>
#include <asm/current.h>
#include <asm/fence.h>

static inline int arch_spin_value_unlocked(arch_spinlock_t lock)
{
        return lock.owner == lock.next;
}

static inline int arch_spin_is_locked(arch_spinlock_t *lock)
{
	return !arch_spin_value_unlocked(READ_ONCE(*lock));
}

static inline int arch_spin_is_contended(arch_spinlock_t *lock)
{
	arch_spinlock_t lockval = READ_ONCE(*lock);
	return (lockval.next - lockval.owner) > 1;
}
#define arch_spin_is_contended	arch_spin_is_contended

static inline int arch_spin_trylock(arch_spinlock_t *lock)
{
	unsigned long inc = 1u << TICKET_SHIFT;
	unsigned long mask = 0xffffu << TICKET_SHIFT;
	u32 l0, tmp1, tmp2;

	__asm__ __volatile__(
		/* Get the current lock counters. */
		"1:	lr.w.aq	%0, %3\n"
		"	slli	%2, %0, %6\n"
		"	and	%2, %2, %5\n"
		"	and	%1, %0, %5\n"
		/* Is the lock free right now? */
		"	bne	%1, %2, 2f\n"
		"	add	%0, %0, %4\n"
		/* Acquire the lock. */
		"	sc.w.rl	%0, %0, %3\n"
		"	bnez	%0, 1b\n"
		"2:"
		: "=&r"(l0), "=&r"(tmp1), "=&r"(tmp2), "+A"(*lock)
		: "r"(inc), "r"(mask), "I"(TICKET_SHIFT)
		: "memory");

	return !l0;
}

static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	unsigned long inc = 1u << TICKET_SHIFT;
	unsigned long mask = 0xffffu;
	u32 l0, tmp1, tmp2;

	__asm__ __volatile__(
		/* Atomically increment the next ticket. */
		"	amoadd.w.aqrl	%0, %4, %3\n"

		/* Did we get the lock? */
		"	srli	%1, %0, %6\n"
		"	and	%1, %1, %5\n"
		"1:	and	%2, %0, %5\n"
		"	beq	%1, %2, 2f\n"

		/* If not, then spin on the lock. */
		"	lw	%0, %3\n"
		RISCV_ACQUIRE_BARRIER
		"	j	1b\n"
		"2:"
		: "=&r"(l0), "=&r"(tmp1), "=&r"(tmp2), "+A"(*lock)
		: "r"(inc), "r"(mask), "I"(TICKET_SHIFT)
		: "memory");
}

static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	__smp_store_release(&lock->owner, READ_ONCE(lock->owner) + 1);
}
/***********************************************************/

static inline void arch_read_lock(arch_rwlock_t *lock)
{
	int tmp;

	__asm__ __volatile__(
		"1:	lr.w	%1, %0\n"
		"	bltz	%1, 1b\n"
		"	addi	%1, %1, 1\n"
		"	sc.w	%1, %1, %0\n"
		"	bnez	%1, 1b\n"
		RISCV_ACQUIRE_BARRIER
		: "+A" (lock->lock), "=&r" (tmp)
		:: "memory");
}

static inline void arch_write_lock(arch_rwlock_t *lock)
{
	int tmp;

	__asm__ __volatile__(
		"1:	lr.w	%1, %0\n"
		"	bnez	%1, 1b\n"
		"	li	%1, -1\n"
		"	sc.w	%1, %1, %0\n"
		"	bnez	%1, 1b\n"
		RISCV_ACQUIRE_BARRIER
		: "+A" (lock->lock), "=&r" (tmp)
		:: "memory");
}

static inline int arch_read_trylock(arch_rwlock_t *lock)
{
	int busy;

	__asm__ __volatile__(
		"1:	lr.w	%1, %0\n"
		"	bltz	%1, 1f\n"
		"	addi	%1, %1, 1\n"
		"	sc.w	%1, %1, %0\n"
		"	bnez	%1, 1b\n"
		RISCV_ACQUIRE_BARRIER
		"1:\n"
		: "+A" (lock->lock), "=&r" (busy)
		:: "memory");

	return !busy;
}

static inline int arch_write_trylock(arch_rwlock_t *lock)
{
	int busy;

	__asm__ __volatile__(
		"1:	lr.w	%1, %0\n"
		"	bnez	%1, 1f\n"
		"	li	%1, -1\n"
		"	sc.w	%1, %1, %0\n"
		"	bnez	%1, 1b\n"
		RISCV_ACQUIRE_BARRIER
		"1:\n"
		: "+A" (lock->lock), "=&r" (busy)
		:: "memory");

	return !busy;
}

static inline void arch_read_unlock(arch_rwlock_t *lock)
{
	__asm__ __volatile__(
		RISCV_RELEASE_BARRIER
		"	amoadd.w x0, %1, %0\n"
		: "+A" (lock->lock)
		: "r" (-1)
		: "memory");
}

static inline void arch_write_unlock(arch_rwlock_t *lock)
{
	smp_store_release(&lock->lock, 0);
}

#endif /* _ASM_RISCV_SPINLOCK_H */
