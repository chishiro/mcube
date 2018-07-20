/**
 * @file arch/x86/dump.c
 *
 * @author Hiroyuki Chishiro
 */
#include <mcube/mcube.h>
//============================================================================
/// @file       dump.c
/// @brief      Debugging memory and CPU state dump routines.
//
//  Copyright 2016 Brett Vickers.
//  Use of this source code is governed by a BSD-style license
//  that can be found in the MonkOS LICENSE file.
//============================================================================


static const char digit[] = "0123456789abcdef";

void dump_registers(const registers_t *regs)
{
  printk("RAX: %lx    RSI: %lx    R11: %lx\n"
         "RBX: %lx    RDI: %lx    R12: %lx\n"
         "RCX: %lx     R8: %lx    R13: %lx\n"
         "RDX: %lx     R9: %lx    R14: %lx\n"
         "RBP: %lx    R10: %lx    R15: %lx\n",
         regs->rax, regs->rsi, regs->r11,
         regs->rbx, regs->rdi, regs->r12,
         regs->rcx, regs->r8, regs->r13,
         regs->rdx, regs->r9, regs->r14,
         regs->rbp, regs->r10, regs->r15);
}

void dump_cpuflags(uint64_t rflags)
{
#define B(F)  ((rflags & F) ? 1 : 0)

  printk("CF=%u   PF=%u   AF=%u   ZF=%u   SF=%u   "
         "TF=%u   IF=%u   DF=%u   OF=%u   IOPL=%lu\n",
         B(CPU_EFLAGS_CARRY), B(CPU_EFLAGS_PARITY), B(CPU_EFLAGS_ADJUST),
         B(CPU_EFLAGS_ZERO), B(CPU_EFLAGS_SIGN), B(CPU_EFLAGS_TRAP),
         B(CPU_EFLAGS_INTERRUPT), B(CPU_EFLAGS_DIRECTION),
         B(CPU_EFLAGS_OVERFLOW), (rflags >> 12) & 3);

#undef B
}

int dump_memory(char *buf, size_t bufsize, const void *mem, size_t memsize,
                enum dumpstyle style)
{
  char          *b  = buf;
  char          *bt = buf + bufsize;
  const uint8_t *m  = (const uint8_t *)mem;
  const uint8_t *mt = (const uint8_t *)mem + memsize;

  while (b < bt && m < mt) {

    // Dump memory offset if requested.
    if (style == DUMPSTYLE_OFFSET) {
      if (b + 11 < bt) {
        uint64_t o = (uint64_t)(m - (const uint8_t *)mem);
        for (int i = 7; i >= 0; i--) {
          b[i] = digit[o & 0xf];
          o  >>= 4;
        }
        b[8] = ':';
        b[9] = b[10] = ' ';
      }
      b += 11;
    }

    // Dump memory address if requested.
    else if (style == DUMPSTYLE_ADDR) {
      if (b + 20 < bt) {
        uint64_t a = (uint64_t)m;
        for (int i = 16; i > 8; i--) {
          b[i] = digit[a & 0xf];
          a  >>= 4;
        }
        b[8] = '`';      // tick separator for readability
        for (int i = 7; i >= 0; i--) {
          b[i] = digit[a & 0xf];
          a  >>= 4;
        }
        b[17] = ':';
        b[18] = b[19] = ' ';
      }
      b += 20;
    }

    // Dump up to 16 hexadecimal byte values.
    for (int j = 0; j < 16; j++) {
      if (b + 2 < bt) {
        if (m + j < mt) {
          uint8_t v = m[j];
          b[0] = digit[v >> 4];
          b[1] = digit[v & 0xf];
        }
        else {
          b[0] = b[1] = ' ';
        }
      }
      b += 2;

      // Add a 1-space gutter between each group of 4 bytes.
      if (((j + 1) & 3) == 0) {
        if (b + 1 < bt) {
          *b = ' ';
        }
        b++;
      }
    }

    // Add a gutter between hex and ascii displays.
    if (b + 1 < bt) {
      *b = ' ';
    }
    b++;

    // Dump up to 16 ASCII bytes.
    for (int j = 0; j < 16; j++) {
      if (b + 1 < bt) {
        if (m + j < mt) {
          uint8_t v = m[j];
          *b = (v < 32 || v > 126) ? '.' : (char)v;
        }
        else {
          *b = ' ';
        }
      }
      b++;

      // Add a gutter between each group of 8 ascii characters.
      if (j == 7) {
        if (b + 1 < bt) {
          *b = ' ';
        }
        b++;
      }
    }

    // Dump a carriage return.
    if (b + 1 < bt) {
      *b = '\n';
    }
    b++;

    m += 16;
  }

  // Null-terminate the buffer.
  if (b < bt) {
    *b = 0;
  } else if (bufsize > 0) {
    b[bufsize - 1] = 0;
  }

  // Return the number of bytes (that would have been) added to the buffer,
  // even if the buffer was too small.
  return (int)(b - buf);
}
