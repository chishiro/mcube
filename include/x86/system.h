/**
 * @file include/x86/system.h
 *
 * @author Hiroyuki Chishiro
 */
#ifndef __MCUBE_X86_SYSTEM_H__
#define __MCUBE_X86_SYSTEM_H__

/* cr0 */
#define CR0_PARING (0x1 << 31)
#define CR0_DISABLE_CACHE (0x1 << 30)
#define CR0_NOT_WRITE_THROUGH (0x1 << 29)
/* 28-19 reserved */
#define CR0_ALIGNMENT_MASK (0x1 << 18)
/* 17 reserved */
#define CR0_WRITE_PROTECT (0x1 << 16)
/* 15-6 reserved */
#define CR0_NUMERIC_ERROR (0x1 << 5)
#define CR0_EXTENSION_TYPE (0x1 << 4)
#define CR0_TASK_SWITCHED (0x1 << 3)
/* enable x87 FPU if clear */
#define CR0_EMULATION (0x1 << 2)
#define CR0_MONITOR_COPROCESSOR (0x1 << 1)
#define CR0_ENABLE_PROTECTED_MODE (0x1 << 0)


#ifndef __ASSEMBLY__


static inline uint64_t get_cr0(void)
{
  uint64_t value;
  asm volatile("mov %0, cr0" : "=r"(value));
  return value;
}

static inline void set_cr0(uint64_t value)
{
  asm volatile("mov cr0, %0" :: "r"(value));
}

/* reserve cr1 */

static inline uint64_t get_cr2(void)
{
  uint64_t value;
  asm volatile("mov %0, cr2" : "=r"(value));
  return value;
}

static inline void set_cr2(uint64_t value)
{
  asm volatile("mov cr2, %0" :: "r"(value));
}

static inline uint64_t get_cr3(void)
{
  uint64_t value;
  asm volatile("mov %0, cr3" : "=r"(value));
  return value;
}

static inline void set_cr3(uint64_t paddr)
{
  asm volatile("mov cr3, %0" :: "r"(paddr));
}

static inline uint64_t get_cr4(void)
{
  uint64_t value;
  asm volatile("mov %0, cr4" : "=r"(value));
  return value;
}

static inline void set_cr4(uint64_t value)
{
  asm volatile("mov cr4, %0" :: "r"(value));
}

static inline uint64_t get_eflags(void)
{
  uint64_t eflags;
  asm volatile("pushfl\n\t"
               "popl %0" : "=r"(eflags));
  return eflags;
}

static inline void set_eflags(uint64_t eflags)
{
  asm volatile("pushl %0 \n\t"
               "popfl" :: "r"(eflags));
}

static inline void generate_software_interrupt(volatile unsigned long id)
{
  //  asm volatile("int %0" :: "r"(id));
}


#endif /* !__ASSEMBLY__ */


#endif /* __MCUBE_X86_SYSTEM_H__ */
