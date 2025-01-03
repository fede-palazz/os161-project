/**
 * @file vm_tlb.c
 * @author L. Mogano
 *
 * This file implements functions for managing the Translation Lookaside Buffer (TLB),
 * including insertion, invalidation, and removal of entries. It uses a round-robin 
 * replacement policy for TLB entries and ensures atomicity by disabling interrupts 
 * during TLB operations.
 *
 */

#include <vm_tlb.h>

/**
 * Current TLB victim pointer for replacement policy.
 *
 * This variable keeps track of the index of the next TLB entry to replace,
 * implementing a round-robin replacement policy.
 */
int tlb_victim = 0;
bool tlb_free = true;

/**
 * @brief Invalidates all TLB entries.
 *
 * This function iterates through all TLB entries and writes invalid values
 * to them, effectively clearing the TLB. Interrupts are disabled during
 * the operation to ensure atomicity.
 */
void
tlb_invalidate(void) {
    int spl, i;

    spl = splhigh(); // Disable interrupts

    /* NUM_TLB is the number of TLB entries in the processor (default to 64) */
    for (i = 0; i < NUM_TLB; i++) {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }
    tlb_victim = 0;
    tlb_free = true;

#if OPT_STATS
    vmstats_hit(VMSTAT_TLB_INVALIDATION);
#endif
    splx(spl); // Restore interrupts
}

/**
 * @brief Inserts a mapping into the TLB.
 *
 * This function writes a mapping between a virtual and physical address into
 * the TLB. The replacement policy uses a round-robin approach. The TLB entry
 * is marked as valid, and the dirty bit is set unless the mapping is read-only.
 *
 * @param vaddr Virtual address to map.
 * @param paddr Physical address to map.
 * @param ro    Boolean indicating if the mapping is read-only.
 */
void
tlb_insert(vaddr_t vaddr, paddr_t paddr, bool ro) {
    int spl;
    uint32_t ehi, elo;

    KASSERT((paddr & PAGE_FRAME) == paddr); // Ensure paddr is page-aligned

    spl = splhigh(); // Saved Processor Level spl -> Disable interrupts
    ehi = vaddr; // Entry High ehi -> Set virtual address
    elo = paddr | TLBLO_VALID; // Entry Low elo -> Set physical address and valid bit
    if (!ro) {
        elo = elo | TLBLO_DIRTY; // Set dirty bit if not read-only
    }
    tlb_write(ehi, elo, tlb_victim); // Write to TLB
     tlb_victim = (tlb_victim + 1) % NUM_TLB; // Update victim index for round-robin
    
#if OPT_STATS
    if(tlb_free)
    {
	    vmstats_hit(VMSTAT_TLB_FAULT_FREE);
    }
    else
    {
        vmstats_hit(VMSTAT_TLB_FAULT_REPLACE);
    }
#endif

    if(tlb_victim == 0)
    {
        tlb_free = false;
    }

    splx(spl); // Restore interrupts
}

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
void 
tlb_remove_by_paddr(paddr_t paddr) {
    // Ensure the physical address is aligned to the page size.
    KASSERT(paddr % PAGE_SIZE == 0);
    
    // Iterate over all TLB entries.
    for (int i = 0; i < NUM_TLB; i++) {
        uint32_t ehi, elo;
        
        // Read the TLB entry at index i.
        tlb_read(&ehi, &elo, i);
        
        // Check if the physical address matches the current TLB entry.
        if (paddr == (elo & PAGE_FRAME)) {
            // Invalidate the matching TLB entry.
            tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
            return;
        }
    }
}