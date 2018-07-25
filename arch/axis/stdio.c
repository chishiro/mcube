/**
 * @file arch/axis/stdio.c
 *
 * @author Hiroyuki Chishiro
 */
#include <mcube/mcube.h>

int putchar(int c)
{
  asm volatile("mts $31, %0" :: "r"(c));
  return c;
}

int puts(const char *s)
{
  while (*s) {
    putchar(*s++);
  }
  return 0;
}
