#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <page_table.h>
#include <vm.h>
#include <frame_table.h>
#include <vm_tlb.h>
#include "opt-smartvm.h"
#include "syscall.h"
#include <swapfile.h>
#include "opt-stats.h"
#include "opt-noswap_rdonly.h"

#if OPT_STATS
// Includes definitions for tracking VM statistics.
#include <vmstats.h>
#endif

#if OPT_SMARTVM
/* under vm, always have 72k of user stack */
/* (this must be > 64K so argument blocks of size ARG_MAX will fit) */

// Bootstrap function to initialize VM-related resources during boot
void
vm_bootstrap(void)
{
#if OPT_SWAP
// Initialize swap file system if swapping is enabled
	swap_bootstrap();
#endif
}

/*
 * Check if we're in a context that can sleep. While most of the
 * operations in vm don't in fact sleep, in a real VM system many
 * of them would. In those, assert that sleeping is ok. This helps
 * avoid the situation where syscall-layer code that works ok with
 * vm starts blowing up during the VM assignment.
 */
static
void
vm_can_sleep(void)
{
	// Check if the current CPU is initialized
	if (CURCPU_EXISTS()) {
		/* must not hold spinlocks */
		KASSERT(curcpu->c_spinlocks == 0);

		/* must not be in an interrupt handler */
		KASSERT(curthread->t_in_interrupt == 0);
	}
}

/**
 * @brief get npages from the frame table.
 * 
 * @param npages 
 * @param ptentry pointer to the pt entry, NULL if kernel's page.
 * @return paddr_t the first physical address of the requested pages.
 */
static
paddr_t
getppages(unsigned long npages, struct pt_entry *ptentry)
{
	paddr_t addr;

	// Requests physical pages from frame table
	addr = frame_table_getppages(npages, ptentry);
	// Check if it is out of memory
	if (addr == 0) {
		panic("Out of memory");
	}

	// Return allocated physical address
	return addr;
}

/**
 * @brief free the allocated pages starting from addr.
 * 
 * @param addr 
 */
static 
void
freeppages(paddr_t addr){
	// Frees pages in the frame table
	frame_table_freeppages(addr);
} 

/* Allocate/free kernel-space virtual pages */
vaddr_t
alloc_kpages(unsigned npages)
{
	paddr_t pa;

	// Ensure the current context allows sleeping
	vm_can_sleep();
	// Allocate physical pages for kernel use
	pa = getppages(npages, NULL);
	// Convert physical address to kernel virtual address
	return PADDR_TO_KVADDR(pa);
}

// Frees kernel-space virtual pages
void
free_kpages(vaddr_t addr)
{
	/* get the physical address */
	// Convert virtual address to physical address
	paddr_t pa = KVADDR_TO_PADDR(addr);
	// Free the physical pages
	freeppages(pa);
}

/**
 * @brief allocate a page for the user. 
 * It is different from the alloc_kpage as it allocate one frame at a time .
 * 
 * @return paddr_t the virtual address of the allocated frame
 */
paddr_t
alloc_upage(struct pt_entry *pt_row){
	paddr_t pa;

	// Ensure the current context allows sleeping
	vm_can_sleep();
	/* the user can alloc one page at a time */
	pa = getppages(1, pt_row);
	// Return the physical address of the allocated page
	return pa;
}

/**
 * @brief deallocate the given page for the user.
 * 
 * @param addr 
 */

void free_upage(paddr_t addr){
	// Free the specified physical page
	freeppages(addr);
};

// Handles TLB shootdown requests 
void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

// Handles page faults by resolving the fault and updating the TLB.
int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	struct pt_entry *pt_row;
	struct addrspace *as;
	paddr_t page_paddr;
	int seg_type;
	int readonly;
	vaddr_t basefaultaddr;

#if OPT_STATS
	// Increment TLB fault statistics
	vmstats_hit(VMSTAT_TLB_FAULT);
#endif

	/* Obtain the first address of the page */
	// Align the fault address to page boundaries
	basefaultaddr = faultaddress & PAGE_FRAME;

	// Log fault address for debugging
	DEBUG(DB_VM, "vm: fault: 0x%x\n", faultaddress);

	// Handle different fault types
	switch (faulttype)
	{
		// Write attempted on a read-only page
 	    case VM_FAULT_READONLY:
			kprintf("vm: got VM_FAULT_READONLY, process killed\n");
			// Terminate the process with an error code
			sys__exit(-1);
			return 0;
			// Read fault
	    case VM_FAULT_READ:
			// Write fault
	    case VM_FAULT_WRITE:
			break;
	    default:
			// Return invalid argument error
			return EINVAL;
	}

	// If no current process exist
	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	// Retrieve the current process's address space
	as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	KASSERT(as->as_data != NULL);
	KASSERT(as->as_stack != NULL);
	KASSERT(as->as_text != NULL);
	KASSERT(as->as_ptable != NULL);

	/**
	 * If as_get_segment_type returns zero, the fault address
	 * does not belong to a valid segment.
	 */
	if(!(seg_type = as_get_segment_type(as, faultaddress))){
		kprintf("vm: got faultaddr out of range, process killed\n");
		sys__exit(-1);
	}
	// Retrieve the page table entry for the fault address
	pt_row = pt_get_entry(as, faultaddress);
	// Mark as read-only if in the text segment
	readonly = seg_type == SEGMENT_TEXT;
	// Handle the page table entry's current status
	switch(pt_row->pt_status)
	{
		// Page is not in memory
		case NOT_LOADED:
			/*	alloc a page				*/
			page_paddr = alloc_upage(pt_row);

			/**  
			 * update page table entry.			
			 * it is important to do it before as_load_page
			 * as the pt_entry will be used to retrieve the
			 * physical address of the page
			 */
			pt_set_entry(pt_row,page_paddr,0, (OPT_NOSWAP_RDONLY && readonly) ? IN_MEMORY_RDONLY : IN_MEMORY); 	

			/*	load the page if needed 	*/
			if(seg_type != SEGMENT_STACK && as_check_in_elf(as,faultaddress))
			{
				// Load the page from the ELF file
				as_load_page(as,curproc->p_vnode,faultaddress);
			}
			
#if OPT_STATS
			else
			{
				// Track zeroed page allocation
    			vmstats_hit(VMSTAT_PAGE_FAULT_ZERO);
			}
#endif
			break;
		case IN_MEMORY_RDONLY:
		// Page is already in memory
		case IN_MEMORY:
#if OPT_STATS
		// Track TLB reload event
    		vmstats_hit(VMSTAT_TLB_RELOAD);
#endif
			break;
		// Page is in swap space
		case IN_SWAP:
#if OPT_SWAP
			/*	alloc the page				*/
			page_paddr = alloc_upage(pt_row);

			/*	swap it from the elf file into memory	*/
			swap_in(page_paddr, pt_row->pt_swap_index);

			/* update page table entry	*/
			
			pt_set_entry(pt_row,page_paddr,0, (OPT_NOSWAP_RDONLY && readonly) ? IN_MEMORY_RDONLY : IN_MEMORY); 

#else
			panic("swap not implemented!");
#endif
		break;
		default:
			panic("Cannot resolve fault");
	}

	// Ensure the segment type is valid
	KASSERT(seg_type != 0);

	/* update tlb	*/
	tlb_insert(basefaultaddr, pt_row->pt_frame_index * PAGE_SIZE, readonly); 

	return 0;
}
#endif /* OPT_SMARTVM */
