/*
 * Optimized C string methods
 *
 * Copyright (C) 2009-2010 Ahmed S. Darwish <darwish.07@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 2.
 *
 * These methods were tested, using LD_PRELOAD, with heavy programs like
 * Firefox, Google Chrome and OpenOffice.
 */

#include <mcube/mcube.h>

/*
 * NOTE: Always put as much logic as possibe out of the inlined assembly
 * code to the asm-constraints C expressions. This gives the optimizer
 * way more freedom, especially regarding constant propagation.
 */

/*
 * GCC "does not parse the assembler instruction template and does not
 * know what it means or even whether it is valid assembler input." Thus,
 * it needs to be told the list of registers __implicitly__ clobbered by
 * such inlined assembly snippets.
 *
 * A good way to stress-test GCC extended assembler constraints is to
 * always inline the assembler snippets (C99 'static inline'), compile
 * the kernel at -O3 or -Os, and roll!
 *
 * Output registers are (explicitly) clobbered by definition. 'CCR' is
 * the x86's condition code register %rflags.
 */

/*
 * The AMD64 ABI guarantees a DF=0 upon function entry.
 */
static void *__memcpy_forward(void *dst, const void *src, size_t len)
{
  uintptr_t d0;

  asm volatile (
    "mov %3, %%rcx;"
    "rep movsb;"      /* rdi, rsi, rcx */
    "mov %4, %%rcx;"
    "rep movsq;"      /* ~~~ */
    :"=&D" (d0), "+&S" (src)
    :"0" (dst), "ir" (len & 7), "ir" (len >> 3)
    :"rcx", "memory");

  return dst;
}

/*
 * We do tolerate overlapping regions here if src > dst. In
 * such case, (src - dst) must be bigger than movsq's block
 * copy factor: 8 bytes.
 */
void *memcpy_forward(void *dst, const void *src, size_t len)
{
  uintptr_t udst, usrc;
  bool bad_overlap;

  udst = (uintptr_t)dst;
  usrc = (uintptr_t)src;

  bad_overlap = (udst + 8 > usrc) && (usrc + len > udst);
  if (__unlikely(bad_overlap))
    panic("%s: badly-overlapped regions, src=0x%lx, dst"
          "=0x%lx, len=0x%lx", __func__, src, dst, len);

  return __memcpy_forward(dst, src, len);
}


/*
 * C99-compliant, with extra sanity checks.
 */
#if 0
void *memcpy(void * restrict dst, const void * restrict src, size_t len)
{
  uintptr_t udst, usrc;
  bool overlap;

  udst = (uintptr_t)dst;
  usrc = (uintptr_t)src;

  overlap = (udst + len > usrc) && (usrc + len > udst);
  if (__unlikely(overlap))
    panic("%s: overlapped regions, src=0x%lx, dst=0x"
          "%lx, len=0x%lx", __func__, src, dst, len);

  return __memcpy_forward(dst, src, len);
}
#endif


/*
 * memcpy(), minus the checks
 *
 * Sanity checks overhead cannot be tolerated for HOT copying
 * paths like screen scrolling.
 *
 * This is also useful for code implicitly called by panic():
 * a sanity check failure there will lead to a stack overflow.
 */

void *memcpy_forward_nocheck(void *dst, const void *src, size_t len)
{
  return __memcpy_forward(dst, src, len);
}

void *memcpy_nocheck(void * restrict dst, const void * restrict src, size_t len)
{
  return __memcpy_forward(dst, src, len);
}


#if STRING_TESTS

#include <kmalloc.h>

static void test_strnlen(const char *str, int len, int expected_len, bool print)
{
  int res;

  res = strnlen(str, len);
  if (res != expected_len)
    panic("_STRING - strnlen(\"%s\", %d) returned %d, while %d "
          "is expected", (print) ? str : "<binary>", len, res,
          expected_len);

  prints("_STRING - strnlen(\"%s\", %d) = %d. Success!\n",
         (print) ? str : "<binary>", len, res);
}

#define _ARRAY_LEN  100
static uint8_t _arr[_ARRAY_LEN];

static void test_memcpy_overlaps(void)
{
  memset(_arr, 0x55, _ARRAY_LEN);

  /* Should succeed */

  memcpy(_arr, _arr + 20, 10);    /* Regular */
  memcpy(_arr + 20, _arr, 10);    /* Regular */
  memcpy(_arr, _arr + 20, 20);    /* Regular, bounds */
  memcpy(_arr + 20, _arr, 20);    /* Regular, bounds */

  memcpy_forward(_arr, _arr + 20, 10);  /* Regular */
  memcpy_forward(_arr + 20, _arr, 10);  /* Regular */
  memcpy_forward(_arr, _arr + 20, 20);  /* Regular, bounds */
  memcpy_forward(_arr + 20, _arr, 20);  /* Regular, bounds */
  memcpy_forward(_arr, _arr + 10, 20);  /* Good overlap */
  memcpy_forward(_arr, _arr + 10, 11);  /* Good overlap, bounds */

  /* Should fail */

  /* memcpy(_arr, _arr + 20, 40);    /\* Overlap *\/ */
  /* memcpy(_arr + 20, _arr, 40);    /\* Overlap *\/ */
  /* memcpy(_arr, _arr + 20, 21);    /\* Overlap, bounds *\/ */
  /* memcpy(_arr + 20, _arr, 21);    /\* Overlap, bounds *\/ */

  /* memcpy_forward(_arr, _arr + 7, 10);  /\* Good overlap, but < 8-byte *\/ */
  /* memcpy_forward(_arr + 20, _arr, 40);  /\* Bad overlap *\/ */
  /* memcpy_forward(_arr + 20, _arr, 21);  /\* Bad overlap, bounds *\/ */
}

void string_run_tests(void)
{
  char *str;
  int i;

  /* Strnlen() tests */

  for (i = 0; i <= 10; i++)
    test_strnlen("", i, 0, true);  /* Bounds, 1 */

  i = 1;
  str = kmalloc(1024);
  for (char ch = 'A'; ch <= 'Z'; ch++, i++) {
    str[i - 1] = ch;
    str[i] = '\0';
    test_strnlen(str, 0, 0, true);  /* Bounds, 2 */
  }

  i = 1;
  for (char ch = 'A'; ch <= 'Z'; ch++, i++) {
    str[i - 1] = ch;
    str[i] = '\0';
    test_strnlen(str, 1024, i, 1);  /* Test it as a regular strlen */
  }

  for (i = 0; i <= 'Z' - 'A' + 1; i++)
    test_strnlen(str, i, i, true);  /* The the 'n' part of strnlen */

  kfree(str);
  memset(_arr, 0x01, _ARRAY_LEN);
  for (i = 0; i <= _ARRAY_LEN; i++)
    test_strnlen((char *)_arr, i, i, 0);   /* Without NULL suffix! */

  /* Memcpy() tests */

  test_memcpy_overlaps();
}

#endif /* STRING_TESTS */
