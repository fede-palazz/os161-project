#include <vm_tlb.h>

/**
 *
 * This file contains the definitions of the functions for managing the TLB,
 * including invalidation and insertion of entries.
 */

/**
 * Current TLB victim pointer for replacement policy.
 *
 * This variable keeps track of the index of the next TLB entry to replace,
 * implementing a round-robin replacement policy.
 */
int victim = 0;

/**
 * Invalidates all TLB entries.
 *
 * This function iterates through all TLB entries and writes invalid values
 * to them, effectively clearing the TLB. Interrupts are disabled during
 * the operation to ensure atomicity.
 */
void tlb_invalidate(void) {
    int spl, i;

    spl = splhigh(); // Disable interrupts
    for (i = 0; i < NUM_TLB; i++) {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }
    splx(spl); // Restore interrupts
}

/**
 *  Inserts a mapping into the TLB.
 *
 * This function writes a mapping between a virtual and physical address into
 * the TLB. The replacement policy uses a round-robin approach. The TLB entry
 * is marked as valid, and the dirty bit is set unless the mapping is read-only.
 *
 * @param vaddr Virtual address to map.
 * @param paddr Physical address to map.
 * @param ro    Boolean indicating if the mapping is read-only.
 */
void tlb_insert(vaddr_t vaddr, paddr_t paddr, bool ro) {
    int spl;
    uint32_t ehi, elo;

    KASSERT((paddr & PAGE_FRAME) == paddr); // Ensure paddr is page-aligned

    spl = splhigh(); // Saved Processor Level spl-> Disable interrupts
    ehi = vaddr; // Entry High ehi-> Set virtual address
    elo = paddr | TLBLO_VALID; // Entry Low elo-> Set physical address and valid bit
    if (!ro) {
        elo = elo | TLBLO_DIRTY; // Set dirty bit if not read-only
    }
    tlb_write(ehi, elo, victim); // Write to TLB
    victim = (victim + 1) % NUM_TLB; // Update victim index for round-robin
    splx(spl); // Restore interrupts
}
