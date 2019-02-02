/**
 * @file include/mcube/kernel.h
 *
 * @author Hiroyuki Chishiro
 */
#ifndef __MCUBE_MCUBE_KERNEL_H__
#define __MCUBE_MCUBE_KERNEL_H__

/*
 * Common methods and definitions
 *
 * Copyright (C) 2009-2012 Ahmed S. Darwish <darwish.07@gmail.com>
 *
 */

#ifndef __ASSEMBLY__

/*
 * GCC extensions shorthands
 */

//#define __aligned(val)  __attribute__((aligned(val)))
#define __pure    __attribute__((pure))
#define __pure_const  __attribute__((const))
#define __packed  __attribute__((packed))
#define __unused  __attribute__((__unused__))
#define __used    __attribute__((__used__))
#define __error(msg)  __attribute__((error(msg)))
#define __likely(exp)  __builtin_expect((exp), 1)
#define __unlikely(exp)  __builtin_expect((exp), 0)
#define __no_inline  __attribute__((noinline))
#define __no_return  __attribute__((noreturn))

/* Mark the 'always_inline' attributed function as C99
 * 'inline' cause the attribute by itself is worthless.
 * It's "for functions declared inline" -- GCC manual */
#ifndef __always_inline
#define __always_inline  inline __attribute__((always_inline))
#endif

/* Suppress GCC's "var used uninitialized" */
#define __uninitialized(x)  (x) = (x)

/*
 * Semi type-safe min and max using GNU extensions
 * The type-checking trick is taken from Linux-2.6.
 */
#define min(x, y) ({                            \
      typeof(x) _m1 = (x);                      \
      typeof(y) _m2 = (y);                      \
      (void) (&_m1 == &_m2);                    \
      _m1 < _m2 ? _m1 : _m2;                    \
    })
#define max(x, y) ({                            \
      typeof(x) _m1 = (x);                      \
      typeof(y) _m2 = (y);                      \
      (void) (&_m1 == &_m2);                    \
      _m1 > _m2 ? _m1 : _m2;                    \
    })
#define swap(x, y) ({                           \
      typeof(x) _m1 = (x);                      \
      typeof(y) _m2 = (y);                      \
      typeof(x) _m3;                            \
      (void) (&_m1 == &_m2);                    \
      _m3 = (x);                                \
      (x) = (y);                                \
      (y) = _m3;                                \
    })


/*
 * In a binary system, a value 'x' is said to be n-byte
 * aligned when 'n' is a power of the radix 2, and x is
 * a multiple of 'n' bytes.
 *
 * A n-byte-aligned value has at least a log2n number of
 * least-significant zeroes.
 *
 * Return given x value 'n'-aligned.
 *
 * Using two's complement, rounding = (x & (typeof(x))-n)
 */
#define round_down(x, n)  (x & ~(typeof(x))(n - 1))
#define round_up(x, n)    (((x - 1) | (typeof(x))(n - 1)) + 1)

/*
 * Check if given 'x' value is 'n'-aligned
 * 'n' must be power of the radix 2; see round_up()
 */
#define __mask(x, n)    ((typeof(x))((n) - 1))
#define is_aligned(x, n)  (((x) & __mask(x, n)) == 0)

/*
 * 'a' Ceil Division 'b'  --  ceil((1.0 * 'a') / 'b')
 *
 * This is a very common operator for idioms like: "Amount
 * of pages needed to render 'x' lines where @a = desired
 * number of lines, and @b = lines in a single page."
 *
 * Or "Number of sectors to hold a a ramdisk image, where
 * @a = ramdisk number of bytes, @b = bytes in a sector.".
 */
static inline uint64_t ceil_div(uint64_t a, uint64_t b)
{
  if (a == 0)
    return a;
  return ((a - 1) / b) + 1;
}

void __no_return panic(const char *fmt, ...);

#if CONFIG_ARCH_SIM

#include <assert.h>

#else

/*
 * C99
 */
#define NULL  ((void *)0)
#define bool  _Bool
#define true    ((_Bool) 1)
#define false   ((_Bool) 0)


#undef EOF
#define EOF -1



#define SUCCESS 1
#define FAILURE 0


/*
 * Main kernel print methods
 */
int vsnprintf(char *buf, int size, const char *fmt, va_list args);
void printk_bust_all_locks(void);
//void printk(const char *fmt, ...);
int prints(const char *fmt, ...);
void putc(char c);
void putc_colored(char c, int color);

void printk_run_tests(void);

/*
 * Critical failures
 */
extern void halt_cpu_ipi_handler(void);

#define assert(condition)                       \
  do {                                          \
    if (__unlikely(!(condition)))               \
      panic("%s:%d - !(" #condition ")\n",      \
            __FILE__, __LINE__);                \
  } while (0);


/*
 * Compiler memory barrier (fence)
 *
 * The 'memory' constraint will "cause GCC to not keep memory
 * values cached in registers across the assembler instruction
 * and not optimize stores or loads to that memory." --GCC
 */
//#define barrier() asm volatile ("":::"memory");

/*
 * For spin-loops, use x86 'pause' and a memory barrier to:
 * - force gcc to reload any values from memory over the busy
 *   loop, avoiding the often-buggy C volatile keyword
 * - hint the CPU to avoid useless memory ordering violations
 * - for Pentium 4, reduce power usage in the busy loop state
 */
#define cpu_pause()                             \
  asm volatile ("pause":::"memory");

/*
 * Compile-time assert for constant-folded expressions
 *
 * We would've been better using GCC's error(msg) attribute,
 * but it doesn't work with my current GCC build :(.
 */
void __unused __undefined_method(void);
#define compiler_assert(condition)              \
  do {                                          \
    if (!(condition))                           \
      __undefined_method();                     \
  } while (0);

#define __arr_size(arr)  (sizeof(arr) / sizeof((arr)[0]))

/*
 * Return length of given array's first dimension.
 *
 * Per C99 spec, the 'sizeof' operator returns an unsigned
 * integer. This is bothersome in the very common idiom:
 *
 *     for (int i = 0; i < ARRAY_SIZE(arr); i++)
 *
 * as it now entails a comparison between a signed and an
 * unsigned value. Thus, _safely_ cast the overall division
 * result below to a signed 32-bit int.
 */
#define ARRAY_SIZE(arr)                                       \
  ({                                                          \
    compiler_assert(__arr_size(arr) <= (uint64_t)INT32_MAX);  \
    (int32_t)__arr_size(arr);                                 \
  })

void __no_return kernel_start(void);

#endif /* !CONFIG_ARCH_SIM */


#endif /* !__ASSEMBLY__ */

#endif /* __MCUBE_MCUBE_KERNEL_H__ */
