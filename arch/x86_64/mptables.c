/**
 * @file arch/x86_64/mptables.c
 *
 * @author Hiroyuki Chishiro
 */
/*
 * Intel MultiProcessor Specification tables parsing
 *
 * Copyright (C) 2009 Ahmed S. Darwish <darwish.07@gmail.com>
 *
 * Note that while the Intel MP spec is sweet, and still in fact
 * supported in these days machines, MOST x86_64 systems are designed
 * with ACPI as the primary model. Everyone knows that ACPI designers
 * should've been buried alive, but we have to live with it.
 */

#include <mcube/mcube.h>

/**
 * @var nr_cpus
 * @brief BIOS-reported usable cores count.
 *
 * The BIOS knows if a core is usable by checking its
 * Builtin-self-test (BIST) result in %rax after RESET#
 */
int nr_cpus = 1;

/**
 * @var cpus[CPUS_MAX]
 * @brief CPU Descriptors Table.
 *
 * Data is gathered from MP or ACPI MADT tables.
 *
 * To make '__current' available to early boot code, it's statically
 * allocated in the first slot. Thus, slot 0 is reserved for the BSC.
 */
struct percpu cpus[CPUS_MAX] = {
  [0] = {
    .__current = &swapper,
  },
};

/*
 * Parsed MP configuration table entries data to be exported
 * to other subsystems
 */

int mp_isa_busid = -1;

int nr_mpcirqs;
struct mpc_irq mp_irqs[MAX_IRQS];

/**
 * @fn static uint8_t mpf_checksum(void *mp, uint32_t len)
 * @brief Checksum method for all MP structures: "All bytes
 * specified by the length field, including the 'checksum'
 * field and reserved bytes, must add up to zero."
 *
 * @param mp MP
 * @param len Length.
 * @return Sum of MP.
 */
static uint8_t mpf_checksum(void *mp, uint32_t len)
{
  uint8_t sum = 0;
  uint8_t *buf = (uint8_t *) mp;

  while (len--) {
    sum += *buf++;
  }

  return sum;
}

/**
 * @fn static struct mpf_struct *search_for_mpf(void *base, uint32_t len)
 * @brief search for MP floating pointer.
 *
 * @param base Base.
 * @param len Length.
 * @return MP pointer if found, and NULL if not found.
 */
static struct mpf_struct *search_for_mpf(void *base, uint32_t len)
{
  struct mpf_struct *mpf = (struct mpf_struct *) base;
  uint8_t checksum;

  for (; len > 0; mpf += 1, len -= sizeof(*mpf)) {
    if (len < sizeof(*mpf)) {
      continue;
    }

    if (mpf->signature != MPF_SIGNATURE) {
      continue;
    }

    if (mpf->length != 0x01) {
      continue;
    }

    if (mpf->version != 0x01 && mpf->version != 0x04) {
      continue;
    }

    checksum = mpf_checksum(mpf, sizeof(*mpf));

    if (checksum != 0) {
      printk("MP: buggy MP floating pointer struct at 0x%lx with checksum = %d\n",
             PHYS(mpf), checksum);
      continue;
    }

    printk("MP: Found an MP pointer at 0x%lx\n", mpf);
    return mpf;
  }

  return NULL;
}

/**
 * @fn static struct mpf_struct *get_mpf(void)
 * @brief Search for the MP floating pointer structure in:
 * - first KB of the extended bios data area (EBDA)
 * - last KB of system base memory (639K-640K)
 * - BIOS ROM address space 0xf0000-0xfffff
 *
 * On AT+ systems, the real-mode segment of the EBDA is
 * stored in the BIOS data area at 0x40:0x0e
 *
 * @return MP floating pointer.
 */
static struct mpf_struct *get_mpf(void)
{
  uintptr_t ebda;
  struct mpf_struct *mpf;

  ebda = (*(uint16_t *) VIRTUAL(0x40e)) << 4;

  if ((mpf = search_for_mpf(VIRTUAL(ebda), 0x400))) {
    return mpf;
  }

  if ((mpf = search_for_mpf(VIRTUAL(639 * 0x400), 0x400))) {
    return mpf;
  }

  if ((mpf = search_for_mpf(VIRTUAL(0xf0000), 0x10000))) {
    return mpf;
  }

  return NULL;
}

/**
 * @fn static int mpc_check(struct mpc_table *mpc)
 * @brief check given MP configuration table header integrity
 *
 * @param mpc MP configuration table.
 * @return 1 if success, and 0 if failure.
 */
static int mpc_check(struct mpc_table *mpc)
{
  uint8_t checksum;

  if (mpc->signature != MPC_SIGNATURE) {
    printk("MP: Wrong configuration table signature = 0x%x\n", mpc->signature);
    return 0;
  }

  if (mpc->version != 0x01 && mpc->version != 0x04) {
    printk("MP: Wrong configuration table version = 0x%x\n", mpc->version);
    return 0;
  }

  checksum = mpf_checksum(mpc, mpc->length);

  if (checksum != 0) {
    printk("MP: buggy configuration table checksum = 0x%x\n", checksum);
    return 0;
  }

  /* Ignore checking the LAPIC base address. Reading the APIC base
   * from the MP table is 12-years obsolete: it was done before
   * Intel's creating of the APIC Base Address MSR in 686+ models */

  return 1;
}


/*
 * Dump MP conf table header
 */
void mpc_dump(struct mpc_table *mpc)
{
  char signature[5];

  signature[4] = 0;
  printk("MP: conf table base = %lx\n", mpc);
  memcpy(signature, &(mpc->signature), 4);
  printk(".. signature = %s\n", signature);
  printk(".. length = %d\n", mpc->length);
  printk(".. version = 0x%x\n", mpc->version);
  printk(".. checksum = 0x%x\n", mpc->checksum);
  printk(".. oem pointer = 0x%x\n", mpc->oem_physaddr);
  printk(".. oem size = 0x%x\n", mpc->oem_size);
  printk(".. entries count = %d\n", mpc->entries);
  printk(".. lapic base = 0x%x\n", mpc->lapic_base);
  printk(".. ext length = %d\n", mpc->ext_length);
  printk(".. ext checksum = 0x%x\n", mpc->ext_checksum);
  printk(".. reserved = 0x%x\n", mpc->reserved);
  printk(".. calculated table checksum = 0x%x\n",
         mpf_checksum(mpc, mpc->length));
  printk(".. calculated extended entries checksum = 0x%x\n",
         mpf_checksum(mpc + 1, mpc->ext_length));
}


/**
 * @fn static void parse_cpu(void *addr)
 * @brief MP base conf table entries parsers.
 * Copy all the needed data to system-wide structures now as
 * we won't parse any of the tables again.
 *
 * @param addr Address.
 */
static void parse_cpu(void *addr)
{
  struct mpc_cpu *cpu = (struct mpc_cpu *) addr;
  static bool bsc_entry_filled;

  if (!cpu->enabled) {
    return;
  }

  if (cpu->bsc) {
    if (bsc_entry_filled) {
      panic("Two `bootstrap' cores in the MP tables! Either the BIOS or our parser is buggy.");
    }

    cpus[0].apic_id = cpu->lapic_id;
    bsc_entry_filled = 1;
    return;
  }

  if (nr_cpus >= CPUS_MAX) {
    panic("Only %d logical CPU cores supported\n", nr_cpus);
  }

  cpus[nr_cpus].apic_id = cpu->lapic_id;

  ++nr_cpus;
}

/**
 * @fn static void parse_ioapic(void *addr)
 * @brief parse I/O APIC.
 *
 * @param addr Address.
 */
static void parse_ioapic(void *addr)
{
  struct mpc_ioapic *ioapic = (struct mpc_ioapic *) addr;

  if (!ioapic->enabled) {
    return;
  }

  if (nr_ioapics >= IOAPICS_MAX) {
    panic("Only %d IO APICs supported", IOAPICS_MAX);
  }

  /* We read the version from the chip itself instead
   * of reading it now from the mptable entries */
  ioapic_descs[nr_ioapics].id = ioapic->id;
  ioapic_descs[nr_ioapics].base = ioapic->base;

  ++nr_ioapics;
}

/**
 * @fn static void parse_irq(void *addr)
 * @brief parse IRQ.
 *
 * @param addr Address.
 */
static void parse_irq(void *addr)
{
  struct mpc_irq *irq = (struct mpc_irq *) addr;

  if (nr_mpcirqs >= MAX_IRQS) {
    panic("Only %d IRQ sources supported", MAX_IRQS);
  }

  mp_irqs[nr_mpcirqs] = *irq;

  ++nr_mpcirqs;
}

/**
 * @fn static void parse_bus(void *addr)
 * @brief parse bus.
 */
static void parse_bus(void *addr)
{
  struct mpc_bus *bus = (struct mpc_bus *) addr;

  /* Only the ISA bus is needed for now */
  if (memcmp("ISA", bus->type, sizeof("ISA") - 1) == 0) {
    mp_isa_busid = bus->id;
  }

  return;
}

/**
 * @fn static int parse_mpc(struct mpc_table *mpc)
 * @brief parse the MP configuration table, copying needed data
 * to our own system-wide mp_*[] tables.
 *
 * @param mpc MP configuration table.
 * @return 1 if success, and 0 if failure.
 */
static int parse_mpc(struct mpc_table *mpc)
{
  uint8_t *entry;

  entry = (uint8_t *)(mpc + 1);  /* WARN: possibly un-mapped! */

  for (int i = 0; i < mpc->entries; i++) {
    entry = (uint8_t *) vm_kmap(PHYS(entry), MPC_ENTRY_MAX_LEN);

    switch (*entry) {
    case MP_PROCESSOR:
      parse_cpu(entry);
      entry += sizeof(struct mpc_cpu);
      break;

    case MP_BUS:
      parse_bus(entry);
      entry += sizeof(struct mpc_bus);
      break;

    case MP_IOAPIC:
      parse_ioapic(entry);
      entry += sizeof(struct mpc_ioapic);
      break;

    case MP_IOINTERRUPT:
      parse_irq(entry);
      entry += sizeof(struct mpc_irq);
      break;

    case MP_LINTERRUPT:
      entry += sizeof(struct mpc_linterrupt);
      break;

    default:
      printk("MP: Unknown conf table entry type = %d\n", *entry);
      return 0;
    }
  }

  return 1;
}

int __pure_const mptables_get_nr_cpus(void)
{
  assert(nr_cpus > 0);

  return nr_cpus;
}

void mptables_init(void)
{
  struct mpf_struct *mpf;
  struct mpc_table *mpc;

  mptables_check();

  mpf = get_mpf();

  if (!mpf) {
    panic("No compliant MP pointer found");
  }

  if (mpf->feature1) {
    panic("MP: Spec `default configuration' is not supported");
  }

  if (mpf->conf_physaddr == 0) {
    panic("MP: Spec configuration table does not exist");
  }

  mpc = (struct mpc_table *) vm_kmap(mpf->conf_physaddr, sizeof(*mpc));

  if (!mpc_check(mpc)) {
    mpc_dump(mpc);
    panic("Buggy MP conf table header");
  }

  /* FIXME: Print MP OEM and Product ID once printk's
   * support for non-null-terminated strings is added */

  if (!parse_mpc(mpc)) {
    mpc_dump(mpc);
    panic("Can not parse MP conf table");
  }
}
