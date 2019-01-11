#ifndef _SPINLOCK_H
#define _SPINLOCK_H

/*
 * SMP spinlocks
 *
 * Copyright (C) 2009 Ahmed S. Darwish <darwish.07@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 2.
 */

#ifndef __ASSEMBLY__

/*
 * Careful! Spinlocks, ironically enough, are globals and thus
 * must be themselves protected against concurrent SMP access!
 */
typedef struct lock_spin {
	uint32_t val;

	/*
	 * Interrupts get disabled when entering spin-protected
	 * critical regions; original state of CPU's ‘IF’ flag
	 * is saved here ... to get restored upon lock release.
	 */
	union x86_rflags rflags;
} spinlock_t;

/* NOTE: mcube's spinlock is reversed. */
#define _SPIN_LOCKED		1
#define _SPIN_UNLOCKED		0

#define SPIN_UNLOCKED()			\
	{				\
		.val = _SPIN_UNLOCKED,	\
	};

void spin_init(spinlock_t *lock);
void spin_lock(spinlock_t *lock);
bool spin_trylock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);

#endif /* !__ASSEMBLY__ */

#endif /* _SPINLOCK_H */
