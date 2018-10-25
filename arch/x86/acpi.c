/**
 * @file arch/x86/acpi.c
 *
 * @author Hiroyuki Chishiro
 */
#include <mcube/mcube.h>
//============================================================================
/// @brief      Advanced configuration and power interface (ACPI) tables.
//
//  Copyright 2016 Brett Vickers.
//  Use of this source code is governed by a BSD-style license
//  that can be found in the MonkOS LICENSE file.
//============================================================================


static struct acpi acpi;

static void read_fadt(const struct acpi_hdr *hdr)
{
  const struct acpi_fadt *fadt = (const struct acpi_fadt *) hdr;
  acpi.fadt = fadt;
}

static void read_madt(const struct acpi_hdr *hdr)
{
  const struct acpi_madt *madt = (const struct acpi_madt *) hdr;
  acpi.madt = madt;
}

static void read_mcfg(const struct acpi_hdr *hdr)
{
  const struct acpi_mcfg *mcfg = (const struct acpi_mcfg *) hdr;
  acpi.mcfg = mcfg;
}

static void read_table(const struct acpi_hdr *hdr)
{
  switch (hdr->signature.dword) {
  case SIGNATURE_FADT:
    read_fadt(hdr);
    break;
  case SIGNATURE_MADT:
    read_madt(hdr);
    break;
  case SIGNATURE_MCFG:
    read_mcfg(hdr);
    break;
  default:
    break;
  }
}

static bool is_mapped(btable_t *btable, uint64_t addr)
{
  uint64_t pml4te = PML4E(addr);
  uint64_t pdpte = PDPTE(addr);
  uint64_t pde = PDE(addr);
  uint64_t pte = PTE(addr);

  page_t *pml4t = btable->root;
  if (pml4t->entry[pml4te] == 0) {
    return FALSE;
  }
  page_t *pdpt = PGPTR(pml4t->entry[pml4te]);
  if (pdpt->entry[pdpte] == 0) {
    return FALSE;
  }
  if (pdpt->entry[pdpte] & PF_PS) {
    return TRUE;
  }

  page_t *pdt = PGPTR(pdpt->entry[pdpte]);
  if (pdt->entry[pde] == 0) {
    return FALSE;
  }
  if (pdt->entry[pde] & PF_PS) {
    return TRUE;
  }

  page_t *pt = PGPTR(pdt->entry[pde]);
  return pt->entry[pte] != 0;
}

static uint64_t alloc_page(btable_t *btable)
{
  if (btable->next_page == btable->term_page) {
    fatal();
  }
  
  page_t *page = btable->next_page++;
  memzero(page, sizeof(page_t));
  return (uint64_t) page | PF_PRESENT | PF_RW;
}

static void create_page(btable_t *btable, uint64_t addr, uint64_t flags)
{
  uint64_t pml4te = PML4E(addr);
  uint64_t pdpte = PDPTE(addr);
  uint64_t pde = PDE(addr);
  uint64_t pte = PTE(addr);

  page_t *pml4t = btable->root;
  if (pml4t->entry[pml4te] == 0) {
    pml4t->entry[pml4te] = alloc_page(btable);
  }
  
  page_t *pdpt = PGPTR(pml4t->entry[pml4te]);
  if (pdpt->entry[pdpte] == 0) {
    pdpt->entry[pdpte] = alloc_page(btable);
  }
  
  page_t *pdt = PGPTR(pdpt->entry[pdpte]);
  if (pdt->entry[pde] == 0) {
    pdt->entry[pde] = alloc_page(btable);
  }
  
  page_t *pt = PGPTR(pdt->entry[pde]);
  pt->entry[pte] = addr | flags;
}

static void map_range(btable_t *btable, uint64_t addr, uint64_t size, uint64_t flags)
{
  // Calculate the page-aligned extents of the block of memory.
  uint64_t begin = PAGE_ALIGN_DOWN(addr);
  uint64_t term  = PAGE_ALIGN_UP(addr + size);

  // If necessary, create new pages in the boot page table to cover the
  // address range.
  for (uint64_t addr = begin; addr < term; addr += PAGE_SIZE) {
    if (!is_mapped(btable, addr)) {
      create_page(btable, addr, flags);
    }
  }
}

static void map_table(btable_t *btable, const struct acpi_hdr *hdr)
{
  uint64_t addr  = (uint64_t) hdr;
  uint64_t flags = PF_PRESENT | PF_RW;

  // First map the header itself, since we can't read its length until
  // it's mapped.
  map_range(btable, addr, sizeof(struct acpi_hdr), flags);

  // Now that we can read the header's length, map the entire ACPI table.
  uint64_t size = hdr->length;
  map_range(btable, addr, size, flags);

  // Calculate the page-aligned extents of the ACPI table, and add them to
  // the BIOS-generated memory table.
  pmap_add(PAGE_ALIGN_DOWN(addr),
           PAGE_ALIGN_UP(addr + hdr->length) - PAGE_ALIGN_DOWN(addr),
           PMEMTYPE_ACPI);
}


static void read_xsdt(btable_t *btable)
{
  const struct acpi_xsdt *xsdt = acpi.xsdt;
  const struct acpi_hdr  *xhdr = &xsdt->hdr;

  print("read_xsdt()\n");
  print("[acpi] oem='%s' tbl='%s' rev=0x%x creator='%s'\n",
         xhdr->oemid, xhdr->oemtableid, xhdr->oemrevision, xhdr->creatorid);

  // Read each of the tables referenced by the XSDT table.
  int tables = (int)(xhdr->length - sizeof(*xhdr)) / sizeof(uint64_t);
  for (int i = 0; i < tables; i++) {
    const struct acpi_hdr *hdr = (const struct acpi_hdr *) xsdt->ptr_table[i];
    map_table(btable, hdr);
    print("[acpi] Found %s table at 0x%lx.\n",
           hdr->signature.bytes, (uint64_t) hdr);
    if (hdr->signature.dword == SIGNATURE_HPET) {
      /* map HPET address space */
      print("HPET\n");
      print("sizeof(struct hpet) = %lu\n", sizeof(struct hpet));
      //      struct hpet *addr = (struct hpet *) HPET0_START;
      struct hpet *addr = (struct hpet *) hdr;
      print("address = 0x%lx\n", addr->address.address);
      pmap_add(PAGE_ALIGN_DOWN(HPET0_START),
               PAGE_ALIGN_UP(HPET0_START + 0x400) - PAGE_ALIGN_DOWN(HPET0_START),
               PMEMTYPE_ACPI);
    }
    read_table(hdr);
  }
  //  inf_loop();
}

static void read_rsdt(btable_t *btable)
{
  const struct acpi_rsdt *rsdt = acpi.rsdt;
  const struct acpi_hdr  *rhdr = &rsdt->hdr;
  int i;
  print("read_rsdt()\n");
  print("[acpi] oem='%s' tbl='%s' rev=%x creator='%s'\n",
         rhdr->oemid, rhdr->oemtableid, rhdr->oemrevision, rhdr->creatorid);

  // Read each of the tables referenced by the RSDT table.
  int tables = (int)(rhdr->length - sizeof(*rhdr)) / sizeof(uint32_t);
  for (i = 0; i < tables; i++) {
    const struct acpi_hdr *hdr = (const struct acpi_hdr *)(uintptr_t) rsdt->ptr_table[i];
    map_table(btable, hdr);
    print("[acpi] Found %s table at 0x%lx.\n",
           hdr->signature.bytes, (uint64_t) hdr);
    print("hdr->length = %u\n", hdr->length);
    print("hdr->signature.bytes = %s\n", hdr->signature.bytes);
    if (hdr->signature.dword == SIGNATURE_HPET) {
      /* map HPET address space */
      print("HPET\n");
      print("sizeof(struct hpet) = %lu\n", sizeof(struct hpet));
      struct hpet *addr = (struct hpet *) hdr;
      print("address = 0x%lx\n", addr->address.address);
      pmap_add(PAGE_ALIGN_DOWN(HPET0_START),
               PAGE_ALIGN_UP(HPET0_START + 0x400) - PAGE_ALIGN_DOWN(HPET0_START),
               PMEMTYPE_ACPI);
    }
    read_table(hdr);
  }
}

static const struct acpi_rsdp *find_rsdp(uint64_t addr, uint64_t size)
{
  // Scan memory for the 8-byte RSDP signature. It's guaranteed to be
  // aligned on a 16-byte boundary.
  const uint64_t *ptr  = (const uint64_t *) addr;
  const uint64_t *term = (const uint64_t *)(addr + size);
  for (; ptr < term; ptr += 2) {
    if (*ptr == SIGNATURE_RSDP) {
      return (const struct acpi_rsdp *) ptr;
    }
  }
  return NULL;
}

void init_acpi(void)
{
  // Initialize the state of the temporary page table generated by the boot
  // loader. We'll be updating it as we scan ACPI tables.
  btable_t btable = {
    .root = (page_t *) MEM_PAGE_TABLE,
    .next_page = (page_t *) MEM_PAGE_TABLE_LOADED,
    .term_page = (page_t *) MEM_PAGE_TABLE_END,
  };

  // Scan the extended BIOS and system ROM memory regions for the ACPI RSDP
  // table.
  if (!(acpi.rsdp = find_rsdp(MEM_EXTENDED_BIOS, MEM_EXTENDED_BIOS_SIZE))) {
    acpi.rsdp = find_rsdp(MEM_SYSTEM_ROM, MEM_SYSTEM_ROM_SIZE);
  }
  // Fatal out if the ACPI tables could not be found.
  if (!acpi.rsdp) {
    print("[acpi] No ACPI tables found.\n");
    fatal();
  }

  acpi.version = acpi.rsdp->revision + 1;
  print("[acpi] ACPI %d.0 RSDP table found at 0x%lx.\n",
         acpi.version, (uintptr_t) acpi.rsdp);

  // Prefer the ACPI2.0 XSDT table for finding all other tables.
  if (acpi.version > 1) {
    acpi.xsdt = (const struct acpi_xsdt *) acpi.rsdp->ptr_xsdt;
    if (!acpi.xsdt) {
      print("[acpi] No XSDT table found.\n");
    } else {
      print("[acpi] Found XSDT table at 0x%lx.\n",
             (uintptr_t) acpi.xsdt);
      map_table(&btable, &acpi.xsdt->hdr);
      read_xsdt(&btable);
    }
  }

  // Fall back to the ACPI1.0 RSDT table if XSDT isn't available.
  if (!acpi.xsdt) {
    if (!(acpi.rsdt = (const struct acpi_rsdt *)(uintptr_t) acpi.rsdp->ptr_rsdt)) {
      print("[acpi] No RSDT table found.\n");
      fatal();
    } else {
      print("[acpi] Found RSDT table at 0x%lx.\n",
             (uintptr_t) acpi.rsdt);
      map_table(&btable, &acpi.rsdt->hdr);
      read_rsdt(&btable);
    }
  }

  // Reserve local APIC memory-mapped I/O addresses.
  if (acpi.madt) {
    //    print("acpi.madt->ptr_local_apic = 0x%x\n", acpi.madt->ptr_local_apic);
    pmap_add(PAGE_ALIGN_DOWN(acpi.madt->ptr_local_apic), PAGE_SIZE,
             PMEMTYPE_UNCACHED);
  }

  // Reserve I/O APIC memory-mapped I/O addresses.
  const struct acpi_madt_io_apic *io = NULL;
  while ((io = acpi_next_io_apic(io))) {
    //    print("io->ptr_io_apic = 0x%x\n", io->ptr_io_apic);
    pmap_add(PAGE_ALIGN_DOWN(io->ptr_io_apic), PAGE_SIZE,
             PMEMTYPE_UNCACHED);
  }
}

int acpi_version(void)
{
  return acpi.version;
}

const struct acpi_fadt *acpi_fadt(void)
{
  return acpi.fadt;
}

const struct acpi_madt *acpi_madt(void)
{
  return acpi.madt;
}

static const void *madt_find(enum acpi_madt_type type, const void *prev)
{
  const struct acpi_madt *madt = acpi.madt;
  if (!madt) {
    return NULL;
  }

  const void *term = (const uint8_t *) madt + madt->hdr.length;

  const void *ptr;
  if (prev) {
    ptr = (const uint8_t *) prev + ((const struct acpi_madt_hdr *) prev)->length;
  } else {
    ptr = madt + 1;
  }

  while (ptr < term) {
    const struct acpi_madt_hdr *hdr = (const struct acpi_madt_hdr *) ptr;
    if (hdr->type == type) {
      return hdr;
    }
    ptr = (const uint8_t *) hdr + hdr->length;
  }

  return NULL;
}

const struct acpi_madt_local_apic *
acpi_next_local_apic(const struct acpi_madt_local_apic *prev)
{
  return (const struct acpi_madt_local_apic *) madt_find(ACPI_MADT_LOCAL_APIC, prev);
}

const struct acpi_madt_io_apic *
acpi_next_io_apic(const struct acpi_madt_io_apic *prev)
{
  return (const struct acpi_madt_io_apic *) madt_find(ACPI_MADT_IO_APIC, prev);
}

const struct acpi_madt_iso *
acpi_next_iso(const struct acpi_madt_iso *prev)
{
  return (const struct acpi_madt_iso *) madt_find(ACPI_MADT_ISO, prev);
}

const struct acpi_mcfg_addr *
acpi_next_mcfg_addr(const struct acpi_mcfg_addr *prev)
{
  const struct acpi_mcfg *mcfg = acpi.mcfg;
  if (!mcfg) {
    return NULL;
  }
  
  const struct acpi_mcfg_addr *ptr;
  if (prev) {
    ptr = prev + 1;
  } else {
    ptr = (const struct acpi_mcfg_addr *)(mcfg + 1);
  }
  
  const uint8_t *term = (const uint8_t *) mcfg + mcfg->hdr.length;
  if ((const uint8_t *) ptr < term) {
    return ptr;
  } else {
    return NULL;
  }
}
