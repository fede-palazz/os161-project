#ifndef _VM_TLB_H_
#define _VM_TLB_H_

#include <types.h>
#include <mips/tlb.h>
#include <lib.h>
#include <spl.h>
#include <vm.h>

#include "opt-stats.h"
#if OPT_STATS
#include <vmstats.h>
#endif

/**
 * This file provides function prototypes for TLB invalidation and insertion.
 * It defines the interface for managing the Translation Lookaside Buffer (TLB).
 */

/**
 * This function invalidates all entries in the TLB, effectively clearing it.
 */
void tlb_invalidate(void);

/**
 * Inserts an entry into the TLB.
 *
 * This function inserts a mapping from a virtual address to a physical address
 * into the TLB. It handles the read-only and dirty flags for memory pages.
 *
 * @param vaddr Virtual address to map.
 * @param paddr Physical address to map.
 * @param ro    Boolean indicating if the mapping is read-only.
 */
void tlb_insert(vaddr_t vaddr, paddr_t paddr, bool ro);

/**
 * @brief Removes a TLB entry that matches the given physical address.
 * 
 * This function iterates over the TLB entries and invalidates the entry 
 * corresponding to the provided physical address. It ensures that the 
 * physical address is aligned to the page size and invalidates the matching entry.
 * 
 * @param paddr Physical address of the page to be invalidated.
 * 
 * The physical address (`paddr`) must be aligned to the page size.
 * If a matching entry is found, it will be invalidated in the TLB.
 * 
 */

void tlb_remove_by_paddr(paddr_t paddr);
#endif /* _VM_TLB_H_ */
