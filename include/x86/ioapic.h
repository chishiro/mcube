/**
 * @file include/x86/ioapic.h
 *
 * @author Hiroyuki Chishiro
 */
#ifndef __MCUBE_X86_IOAPIC_H__
#define __MCUBE_X86_IOAPIC_H__

/*
 * I/O APIC definitions
 *
 * Copyright (C) 2009 Ahmed S. Darwish <darwish.07@gmail.com>
 */


#ifndef __ASSEMBLY__

/*
 * System-wide I/O APIC descriptors for each I/O APIC reported
 * by the BIOS as usable. At lease one ioapic should be enabled.
 */

struct ioapic_desc {
  uint8_t  id;      /* Chip APIC ID */
  uint8_t version;    /* Chip version: 0x11, 0x20, .. */
  uint32_t base;      /* This IOAPIC physical base address */
  uint8_t max_irq;    /* IRQs = (0 - max_irq) inclusive */
};

extern int nr_ioapics;      /* (MP, ACPI) usable I/O APICs count */
#define IOAPICS_MAX  8    /* Arbitary */

/* FIXME: locks around the global resource array once SMP
 * primitive locking is done */
extern struct ioapic_desc ioapic_descs[IOAPICS_MAX];

/*
 * I/O APIC registers and register accessors
 */

#define IOAPIC_ID  0x00
union ioapic_id {
  uint32_t value;
  struct {
    uint32_t reserved0:24, id:8;
  } __packed;
};

#define IOAPIC_VER  0x01
union ioapic_ver {
  uint32_t value;
  struct {
    uint32_t version:8, reserved0:8,
      max_irq:8, reserved1:8;
  } __packed;
};

#define IOAPIC_ARB  0x02
union ioapic_arb {
  uint32_t value;
  struct {
    uint32_t reserved0:24, arbitration:4,
      reserved1:4;
  } __packed;
};

/*
 * IOAPIC memory mapped registers address space size
 */
#define IOAPIC_MMIO_SPACE  0x20

/*
 * Get the IO APIC virtual base from the IOAPICs repository.
 * The data was found either by parsing the MP-tables or ACPI.
 */
static inline uintptr_t ioapic_base(int apic)
{
  void *vbase;

  assert(apic < nr_ioapics);
  vbase = vm_kmap(ioapic_descs[apic].base, IOAPIC_MMIO_SPACE);

  return (uintptr_t)vbase;
}

static inline uint32_t ioapic_read(int apic, uint8_t reg)
{
  uint32_t *ioregsel = (uint32_t *)ioapic_base(apic);
  uint32_t *iowin = (uint32_t *)(ioapic_base(apic) + 0x10);

  writel(reg, ioregsel);
  return readl(iowin);
}

static inline void ioapic_write(int apic, uint8_t reg, uint32_t value)
{
  uint32_t *ioregsel = (uint32_t *)ioapic_base(apic);
  uint32_t *iowin = (uint32_t *)(ioapic_base(apic) + 0x10);

  writel(reg, ioregsel);
  writel(value, iowin);
}

#define IOAPIC_REDTBL0  0x10
/* Don't use a single uint64_t element here. All APIC registers are
 * accessed using 32 bit loads and stores. Registers that are
 * described as 64 bits wide are accessed as multiple independent
 * 32 bit registers -- Intel 82093AA datasheet */
union ioapic_irqentry {
  struct {
    uint32_t vector:8, delivery_mode:3, dest_mode:1,
      delivery_status:1, polarity:1, remote_irr:1,
      trigger:1, mask:1, reserved0:15;

    uint32_t reserved1:24, dest:8;
  } __packed;
  struct {
    uint32_t value_low;
    uint32_t value_high;
  } __packed;
  uint64_t value;
};

/* Delivery mode (R/W) */
enum ioapic_delmod {
  IOAPIC_DELMOD_FIXED = 0x0,
  IOAPIC_DELMOD_LOWPR = 0x1,
  IOAPIC_DELMOD_SMI   = 0x2,
  IOAPIC_DELMOD_NMI   = 0x4,
  IOAPIC_DELMOD_INIT  = 0x5,
  IOAPIC_DELMOD_EXTINT= 0x7,
};
/* Destination mode (R/W) */
enum ioapic_destmod {
  IOAPIC_DESTMOD_PHYSICAL = 0x0,
  IOAPIC_DESTMOD_LOGICAL  = 0x1,
};
/* Interrupt Input Pin Polarity (R/W) */
enum ioapic_polarity {
  IOAPIC_POLARITY_HIGH = 0x0,
  IOAPIC_POLARITY_LOW  = 0x1,
};
/* Trigger Mode (R/W) */
enum ioapic_trigger {
  IOAPIC_TRIGGER_EDGE  = 0x0,
  IOAPIC_TRIGGER_LEVEL = 0x1,
};
/* Interrupt Mask (R/W) */
enum {
  IOAPIC_UNMASK = 0x0,
  IOAPIC_MASK   = 0x1,
};
/* Message Destination Address (APIC logical destination mode) */
enum {
   /* Each local APIC performs a logical AND of chosen
    * address and its logical APIC ID. If a 'true'
    * condition was detected, the IRQ is accepted. */
  IOAPIC_DEST_BROADCAST = 0xff,
};

static inline union ioapic_irqentry ioapic_read_irqentry(int apic, uint8_t irq)
{
  union ioapic_irqentry entry = { .value = 0 };
  entry.value_low = ioapic_read(apic, IOAPIC_REDTBL0 + 2*irq);
  entry.value_high = ioapic_read(apic, IOAPIC_REDTBL0 + 2*irq + 1);
  return entry;
}

/*
 * NOTE! Write the upper half _before_ writing the lower half.
 * The low word contains the mask bit, and we want to be sure
 * of the irq entry integrity if the irq is going to be enabled.
 */
static inline void ioapic_write_irqentry(int apic, uint8_t irq,
                                         union ioapic_irqentry entry)
{
  ioapic_write(apic, IOAPIC_REDTBL0 + 2*irq + 1, entry.value_high);
  ioapic_write(apic, IOAPIC_REDTBL0 + 2*irq, entry.value_low);
}

static inline void ioapic_mask_irq(int apic, uint8_t irq)
{
  union ioapic_irqentry entry = { .value = 0 };
  entry.value_low = ioapic_read(apic, IOAPIC_REDTBL0 + 2*irq);
  entry.mask = IOAPIC_MASK;
  ioapic_write(apic, IOAPIC_REDTBL0 + 2*irq, entry.value_low);
}

/*
 * Represents where an interrupt source is connected to the
 * I/O APICs system
 */
struct ioapic_pin {
  int apic;      /* which ioapic? */
  int pin;      /* which pin in this ioapic */
};

void ioapic_setup_isairq(uint8_t irq, uint8_t vector, enum irq_dest);
void ioapic_init(void);

#endif /* !__ASSEMBLY__ */

#endif /* __MCUBE_X86_IOAPIC_H__ */
