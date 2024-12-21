#ifndef _VM_TLB_H_
#define _VM_TLB_H_

#include <types.h>
#include <mips/tlb.h>
#include <lib.h>
#include <spl.h>
#include <vm.h>

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
 * Removes a TLB entry corresponding to a virtual address.
 *
 * This function searches for a TLB entry that matches the given virtual address
 * and invalidates it if found.
 *
 * @param vaddr Virtual address whose mapping should be removed.
 */
void tlb_remove(vaddr_t vaddr);
#endif /* _VM_TLB_H_ */
