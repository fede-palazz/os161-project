#include <vm_tlb.h>
#include <mips/tlb.h>
#include <spl.h>
#include <vm.h>
#include <lib.h>
#include "opt-stats.h"
#if OPT_STATS
#include <vmstats.h>
#endif

//Initialize the TLB victim pointer to 0 for replacement
int tlb_victim = 0;
 // Flag to indicate if the TLB contains free entries
bool tlb_free = true;

void tlb_invalidate(void)
{
    // Declaration of variables `spl` for saving interrupt state, `i` for iteration
    int spl, i;

    /* Disable interrupts on this CPU while frobbing the TLB. */
    // Raise the interrupt priority level to prevent interruptions during TLB operations.
    spl = splhigh();

    // Iterate over all TLB entries and invalidate each TLB entry by writing invalid values
    for (i = 0; i < NUM_TLB; i++)
    {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }

    // Reset the TLB victim pointer to the start
    tlb_victim = 0;
    // Mark the TLB as containing free entries.
    tlb_free = true;

#if OPT_STATS
    //Log a TLB invalidation event if statistics are enabled
    vmstats_hit(VMSTAT_TLB_INVALIDATION);
#endif

    // Restore the previous interrupt priority level.
    splx(spl);
}

void tlb_insert(vaddr_t vaddr, paddr_t paddr, bool ro)
{
    // Variable to save the interrupt state (as used above)
    int spl;
    // Variables for TLB entry high and low values
    uint32_t ehi, elo;

    /* Make sure it's page-aligned */
    KASSERT((paddr & PAGE_FRAME) == paddr);

    /* Disable interrupts on this CPU while frobbing the TLB. */
   // Raise interrupt priority to prevent interruptions during TLB modification
    spl = splhigh();

    // Set the TLB entry high to the virtual address
    ehi = vaddr;
    // Set the TLB entry low to the physical address and mark it valid
    elo = paddr | TLBLO_VALID;
    // Check if the mapping is writable
    if (!ro)
    {
        // Mark the entry as writable if not read-only
        elo = elo | TLBLO_DIRTY;
    }
    // Write the entry to the TLB at the victim index
    tlb_write(ehi, elo, tlb_victim);
    // Move the victim pointer to the next entry, wrapping around
    tlb_victim = (tlb_victim + 1) % NUM_TLB;

#if OPT_STATS
    // Check if there are free TLB entries
    if(tlb_free)
    {
        // Log a free TLB fault event if statistics are enabled
	    vmstats_hit(VMSTAT_TLB_FAULT_FREE);
    }
    else
    {
        // Log a replacement TLB fault event if statistics are enabled
        vmstats_hit(VMSTAT_TLB_FAULT_REPLACE);
    }
#endif
    // Check if the victim pointer has wrapped back to the start
    if(tlb_victim == 0)
    {
        // Mark the TLB as no longer having free entries
        tlb_free = false;
    }

    // Restore the previous interrupt priority level
    splx(spl);
}


void tlb_remove_by_paddr(paddr_t paddr) {
    
    // Assert that the physical address is page-aligned
    KASSERT(paddr % PAGE_SIZE == 0);

    // Iterate over all TLB entries
    for (int i = 0; i < NUM_TLB; i++) {
        // Variables for TLB entry high and low values
        uint32_t ehi, elo;
        // Read the current TLB entry at index `i`
        tlb_read(&ehi, &elo, i);
        // Check if the physical address matches the TLB entry
        if (paddr == (elo & PAGE_FRAME)) {
            // Invalidate the matching TLB entry
            tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
            // Exit the function after invalidating the entry
            return;
        }
    }
}