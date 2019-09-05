/**
 * @file arch/x86_64/e820_map.c
 *
 * @author Hiroyuki Chishiro
 */
/*
 * E820h maps (Check e820.h)
 *
 * Copyright (C) 2010 Ahmed S. Darwish <darwish.07@gmail.com>
 *
 * In an earlier stage, we've switched to real-mode and acquired the
 * BIOS's ACPI E820h memory map, which includes details on available and
 * reserved memory pages. The real-mode code passes the map entries to
 * the rest of the kernel in a below 1MB structure defined in e820.h
 *
 * Here (and at e820.h) we export all of those details in a friendly
 * manner to the rest of the kernel.
 */

#include <mcube/mcube.h>

/**
 * @var memory_setup
 * @brief Memory setup.
 */
static struct e820_setup memory_setup;

/**
 * @var e820_errors[]
 * @brief E820h struct error to string map.
 */
static const char *e820_errors[] = {
  "success", /* [E820_SUCCESS]  */
  "no BIOS support", /* [E820_NOT_SUPP] */
  "custom buffer full", /* [E820_BUF_FULL] */
  "general error (carry set)", /* [E820_ERROR] */
  "BIOS bug, violating ACPI", /* [E820_BIOS_BUG] */
  "huge returned e820 entry" /* [E820_HUGE_ENTRY] */
};

/**
 * @fn static const char *e820_errstr(uint32_t error)
 * @brief E820 error to string.
 *
 * @param error Error.
 * @return String.
 */
static const char *e820_errstr(uint32_t error)
{
  if (error > E820_HUGE_ENTRY) {
    return "unknown e820.S-reported error";
  }

  assert(error < ARRAY_SIZE(e820_errors));

  return e820_errors[error];
}

/**
 * @var e820_types[]
 * @brief ACPI memory type -> string.
 */
static const char *e820_types[] = {
  "available", /* [E820_AVAIL] */
  "reserved", /* [E820_RESERVED] */
  "acpi tables", /* [E820_ACPI_TBL] */
  "acpi nvs", /* [E820_ACPI_NVS] */
  "erroneous", /* [E820_ERRORMEM] */
  "disabled" /* [E820_DISABLED] */
};

/**
 * @fn static const char *e820_typestr(uint32_t type)
 * @brief transform given ACPI type value to string.
 *
 * @param type Type.
 * @return String.
 */
static const char *e820_typestr(uint32_t type)
{
  if (type < E820_AVAIL || type > E820_DISABLED) {
    return "unknown type - reserved";
  }

  /* Don't put this on top of above return; we
   * can have unknown values from future BIOSes */
  assert(type < ARRAY_SIZE(e820_types));

  return e820_types[type];
}


/**
 * @fn static uint32_t e820_checksum(void *base, int len)
 * @brief checksum the rmode-returned e820h struct
 *
 * @param base Base.
 * @param len Length.
 */
static uint32_t e820_checksum(void *base, int len)
{
  uint8_t *p;
  uint32_t sum;

  p = (uint8_t *) base;
  sum = 0;

  while (len--) {
    sum += *p++;
  }

  return sum;
}

/**
 * @fn static void validate_e820h_struct(void)
 * @brief check if real-mode returned E820h-struct is correctly formed.
 * Valid formation cues include the start signa-
 * ture, number of table entries, and the checksum.
 * NOTE! We don't check struct _entries_ validity here.
 */
static void validate_e820h_struct(void)
{
  uint32_t *entry, entry_len, err, chksum1, chksum2;
  struct e820_range *range;

  entry = (uint32_t *) E820_BASE;

  if (*entry != E820_INIT_SIG) {
    panic("E820h - Invalid buffer start signature");
  }

  entry++;

  while (*entry != E820_END) {
    if (entry >= (uint32_t *)E820_MAX) {
      panic("E820h - Unterminated buffer structure");
    }

    entry_len = *entry++;
    range = (struct e820_range *) entry;
    printk("Memory: E820 range: 0x%lx - 0x%lx (%s)\n", range->base,
           range->base + range->len, e820_typestr(range->type));

    entry = (uint32_t *)((char *) entry + entry_len);
  }

  entry++;

  err = *entry;

  if (err != E820_SUCCESS) {
    panic("E820h error - %s", e820_errstr(err));
  }

  entry++;

  chksum2 = *entry;
  chksum1 = e820_checksum(E820_BASE, (char *)entry - (char *)E820_BASE);

  if (chksum1 != chksum2) {
    panic("E820h error - calculated checksum = 0x%lx, "
          "found checksum = 0x%lx\n", chksum1, chksum2);
  }

  entry++;

  assert(entry <= (uint32_t *)E820_MAX);

  /* Things are hopefully fine; mark the struct as valid */
  entry = (uint32_t *) E820_BASE;
  *entry = E820_VALID_SIG;
}

/**
 * @fn static void build_memory_setup(void)
 * @brief build memory setup.
 */
static void build_memory_setup(void)
{
  uint64_t avail_len, avail_ranges, phys_end, end;
  struct e820_range *range = NULL;

  assert(memory_setup.valid == 0);

  phys_end = 0;
  avail_len = 0;
  avail_ranges = 0;
  e820_for_each(range) {
    if (range->type != E820_AVAIL) {
      continue;
    }

    avail_len += range->len;
    avail_ranges++;

    end = range->base + range->len;

    if (end > phys_end) {
      phys_end = end;
    }
  }

  memory_setup.valid = 1;
  memory_setup.avail_ranges = avail_ranges;
  memory_setup.avail_pages = avail_len / PAGE_SIZE;
  memory_setup.phys_addr_end = phys_end;
}

int e820_sanitize_range(struct e820_range *range, uint64_t kmem_end)
{
  uint64_t start, end;

  assert(range->type == E820_AVAIL);
  start = range->base;
  end = start + range->len;

  start = ROUND_UP(start, PAGE_SIZE);
  end = ROUND_DOWN(end, PAGE_SIZE);

  if (end <= start) {
    range->type = E820_ERRORMEM;
    return -1;
  }

  assert(page_aligned(kmem_end));

  if (end <= PHYS(kmem_end)) {
    return -1;
  }

  if (start < PHYS(kmem_end)) {
    start = PHYS(kmem_end);
  }

  range->base = start;
  range->len = end - start;

  return 0;
}

struct e820_setup *e820_get_memory_setup(void)
{
  assert(memory_setup.valid == 1);

  return &memory_setup;
}

uint64_t e820_get_phys_addr_end(void)
{
  assert(memory_setup.valid == 1);
  assert(memory_setup.phys_addr_end);

  return memory_setup.phys_addr_end;
}

void e820_init(void)
{
  validate_e820h_struct();
  build_memory_setup();
}
