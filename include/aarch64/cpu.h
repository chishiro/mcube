/**
 * @file include/aarch64/cpu.h
 *
 * @author Hiroyuki Chishiro
 */
#ifndef __MCUBE_AARCH64_CPU_H__
#define __MCUBE_AARCH64_CPU_H__


/**
 * @def USER_LEVEL
 * @brief User level.
 */
#define USER_LEVEL 0

/**
 * @def KERNEL_LEVEL
 * @brief Kernel level.
 */
#define KERNEL_LEVEL 1

/**
 * @def HYPERVISOR_LEVEL
 * @brief Hypervisor level.
 */
#define HYPERVISOR_LEVEL 2

/**
 * @def TRUST_ZONE_LEVEL
 * @brief Trust zone level.
 */
#define TRUST_ZONE_LEVEL 3

/**
 * @def REG_LENGTH
 * @brief Register length.
 */
#define REG_LENGTH 64

#ifdef __ASSEMBLY__

.macro get_cpu_id reg reg2
mrs \reg2, mpidr_el1
lsr \reg, \reg2, #8
and \reg2, \reg2, #0xffffff
and \reg, \reg, #0xff000000
orr \reg, \reg, \reg2
.endm

.macro get_el
mrs x0, CurrentEL
lsr x0, x0, #2
ret
.endm

#else


/**
 * @fn static inline unsigned long get_cpu_id(void)
 * @brief get CPU ID.
 *
 * @return CPU ID.
 */
static inline unsigned long get_cpu_id(void)
{
  unsigned long reg;
  asm volatile("mrs %0, mpidr_el1" : "=r"(reg));
  reg = ((reg >> 8) & 0x00000000ff000000) | (reg & 0x0000000000ffffff);
  return reg;
}

/**
 * @fn static inline unsigned long get_el(void)
 * @brief get Exception Level (EL).
 */
static inline unsigned long get_el(void)
{
  unsigned long el;
  asm volatile("mrs %0, CurrentEL" : "=r"(el));
  return el >> 2;
}


#endif /* !__ASSEMBLY__ */


#endif /* __MCUBE_AARCH64_CPU_H__ */
