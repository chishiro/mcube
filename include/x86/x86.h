#ifndef _X86_H
#define _X86_H

/*
 * General x86 properties
 *
 * Copyright (C) 2009 Ahmed S. Darwish <darwish.07@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 2.
 */

#define MSR_FS_BASE	0xC0000100
#define MSR_GS_BASE	0xC0000101

#ifndef __ASSEMBLY__


union x86_rflags {
	uint64_t raw;
	struct {
		uint32_t carry_flag:1,		/* Last math op resulted carry */
			__reserved1_0:1,	/* Always 1 */
			parity_flag:1,		/* Last op resulted even parity */
			__reserved0_0:1,	/* Must be zero */
			auxiliary_flag:1,	/* BCD carry */
			__reserved0_1:1,	/* Must be zero */
			zero_flag:1,		/* Last op resulted zero */
			sign_flag:1,		/* Last op resulted negative */
			trap_flag:1,		/* Enable single-step mode */
			irqs_enabled:1,		/* Maskable interrupts enabled? */
			direction_flag:1,	/* String ops direction */
			overflow_flag:1,	/* Last op MSB overflowed */
			io_privilege:2,		/* IOPL of current task */
			nested_task:1,		/* Controls chaining of tasks */
			__reserved0_2:1,	/* Must be zero */
			resume_flag:1,		/* Debug exceptions related */
			virtual_8086:1,		/* Enable/disable 8086 mode */
			alignment_check:1,	/* Enable alignment checking? */
			virtual:2,		/* Virtualization fields */
			id_flag:1,		/* CPUID supported if writable */
			__reserved0_3:10;	/* Must be zero */
		uint32_t __reserved0_4;		/* Must be zero */
	} __packed;
};

static inline union x86_rflags get_rflags(void)
{
	union x86_rflags flags;

	asm volatile ("pushfq;"
		      "popq %0;"
		      :"=rm"(flags.raw)
		      :);

	return flags;
}

/*
 * Setting %rflags may enable interrupts, but we often want to
 * do so in the _exact_ location specified: e.g. spin_unlock()
 * should be compiled to mark the lock as available (lock->val
 * = 1) before enabling interrupts, but never after.
 */
static inline void set_rflags(union x86_rflags flags)
{
	asm volatile ("pushq %0;"
		      "popfq;"
		      :
		      :"g"(flags.raw)
		      :"cc", "memory");
}

/*
 * Default rflags: set it to rflags of new threads, etc
 * This is same as the CPU rflags value following #RESET or
 * INIT SIPI, with the difference of having IRQs enabled.
 */
static inline union x86_rflags default_rflags(void)
{
	union x86_rflags flags;

	flags.raw = 0;
	flags.__reserved1_0 = 1;
	flags.irqs_enabled = 1;

	return flags;
}

/*
 * The given FS.base and GS.base values must be in canonical
 * form or a general-protection (#GP) exception will occur.
 */

static inline void set_fs(uint64_t val)
{
	write_msr(MSR_FS_BASE, val);
}

static inline void set_gs(uint64_t val)
{
	write_msr(MSR_GS_BASE, val);
}

static inline uint64_t get_gs(void)
{
	return read_msr(MSR_GS_BASE);
}

#endif /* !__ASSEMBLY__ */
#endif /* _X86_H */
