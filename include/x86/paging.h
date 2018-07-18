/**
 * @file include/x86/paging.h
 *
 * @author Hiroyuki Chishiro
 */
#ifndef __MCUBE_X86_PAGING_H__
#define	__MCUBE_X86_PAGING_H__
//============================================================================
/// @file       paging.h
/// @brief      Paged memory management.
//
// Copyright 2016 Brett Vickers.
// Use of this source code is governed by a BSD-style license that can
// be found in the MonkOS LICENSE file.
//============================================================================

#ifndef __ASSEMBLY__


// Page size constants
#define PAGE_SIZE        0x1000
#define PAGE_SIZE_LARGE  0x200000
#define PAGE_SIZE_HUGE   0x40000000

// Page table entry flags
#define PF_PRESENT       (1 << 0)   // Page is present in the table
#define PF_RW            (1 << 1)   // Read-write
#define PF_USER          (1 << 2)   // User-mode (CPL==3) access allowed
#define PF_PWT           (1 << 3)   // Page write-thru
#define PF_PCD           (1 << 4)   // Cache disable
#define PF_ACCESS        (1 << 5)   // Indicates whether page was accessed
#define PF_DIRTY         (1 << 6)   // Indicates whether 4K page was written
#define PF_PS            (1 << 7)   // Page size (valid for PD and PDPT only)
#define PF_GLOBAL        (1 << 8)   // Indicates the page is globally cached
#define PF_SYSTEM        (1 << 9)   // Page used by the kernel

// Virtual address bitmasks and shifts
#define PGSHIFT_PML4E    39
#define PGSHIFT_PDPTE    30
#define PGSHIFT_PDE      21
#define PGSHIFT_PTE      12
#define PGMASK_ENTRY     0x1ff
#define PGMASK_OFFSET    0x3ff

// Virtual address subfield accessors
#define PML4E(a)         (((a) >> PGSHIFT_PML4E) & PGMASK_ENTRY)
#define PDPTE(a)         (((a) >> PGSHIFT_PDPTE) & PGMASK_ENTRY)
#define PDE(a)           (((a) >> PGSHIFT_PDE) & PGMASK_ENTRY)
#define PTE(a)           (((a) >> PGSHIFT_PTE) & PGMASK_ENTRY)

// Page table entry helpers
#define PGPTR(pte)       ((page_t *)((pte) & ~PGMASK_OFFSET))

//----------------------------------------------------------------------------
//  @union      page_t
/// @brief      A pagetable page record.
/// @details    Contains 512 page table entries if the page holds a page
///             table. Otherwise it contains 4096 bytes of memory.
//----------------------------------------------------------------------------
typedef union page
{
    uint64_t entry[PAGE_SIZE / sizeof(uint64_t)];
    uint8_t  memory[PAGE_SIZE];
} page_t;

//----------------------------------------------------------------------------
//  @struct     pagetable_t
/// @brief      A pagetable structure.
/// @details    Holds all the page table entries that map virtual addresses to
///             physical addresses.
//----------------------------------------------------------------------------
typedef struct pagetable
{
    uint64_t proot;     ///< Physical address of root page table (PML4T) entry
    uint64_t vroot;     ///< Virtual address of root page table (PML4T) entry
    uint64_t vnext;     ///< Virtual address to use for table's next page
    uint64_t vterm;     ///< Boundary of pages used to store the table
} pagetable_t;

//----------------------------------------------------------------------------
//  @function   page_init
/// @brief      Initialize the page frame database.
/// @details    The page frame database manages the physical memory used by
///             all memory pages known to the kernel.
//----------------------------------------------------------------------------
void
page_init();

//----------------------------------------------------------------------------
//  @function   pagetable_create
/// @brief      Create a new page table that can be used to associate virtual
///             addresses with physical addresses. The page table includes
///             protected mappings for kernel memory.
/// @param[in]  pt      A pointer to the pagetable structure that will hold
///                     the page table.
/// @param[in]  vaddr   The virtual address within the new page table where
///                     the page table will be mapped.
/// @param[in]  size    Maximum size of the page table in bytes. Must be a
///                     multiple of PAGE_SIZE.
/// @returns    A handle to the created page table.
//----------------------------------------------------------------------------
void
pagetable_create(pagetable_t *pt, void *vaddr, uint64_t size);

//----------------------------------------------------------------------------
//  @function   pagetable_destroy
/// @brief      Destroy a page table.
/// @param[in]  pt      A handle to the page table to destroy.
//----------------------------------------------------------------------------
void
pagetable_destroy(pagetable_t *pt);

//----------------------------------------------------------------------------
//  @function   pagetable_activate
/// @brief      Activate a page table on the CPU, so all virtual memory
///             operations are performed relative to the page table.
/// @param[in]  pt      A handle to the activated page table. Pass NULL to
///                     activate the kernel page table.
//----------------------------------------------------------------------------
void
pagetable_activate(pagetable_t *pt);

//----------------------------------------------------------------------------
//  @function   page_alloc
/// @brief      Allocate one or more pages contiguous in virtual memory.
/// @param[in]  pt      Handle to the page table from which to allocate the
///                     page(s).
/// @param[in]  vaddr   The virtual address of the first allocated page.
/// @param[in]  count   The number of contiguous virtual memory pages to
///                     allocate.
/// @returns    A virtual memory pointer to the first page allocated.
//----------------------------------------------------------------------------
void *
page_alloc(pagetable_t *pt, void *vaddr, int count);

//----------------------------------------------------------------------------
//  @function   page_free
/// @brief      Free one or more contiguous pages from virtual memory.
/// @param[in]  pt      Handle to ehte page table from which to free the
///                     page(s).
/// @param[in]  vaddr   The virtual address of the first allocated page.
/// @param[in]  count   The number of contiguous virtual memory pages to free.
//----------------------------------------------------------------------------
void
page_free(pagetable_t *pt, void *vaddr, int count);

#endif /* !__ASSEMBLY__ */

#endif	/* __MCUBE_X86_PAGING_H__ */
