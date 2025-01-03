#include <vm.h>

#if OPT_SMARTVM
/* under vm, always have 72k of user stack */
/* (this must be > 64K so argument blocks of size ARG_MAX will fit) */


static struct spinlock vm_lock = SPINLOCK_INITIALIZER;

void
vm_bootstrap(void)
{
	#if OPT_SWAP
		swap_bootstrap();
	#endif
}

/**
 * @brief Check if the current context can safely sleep.
 * 
 * Ensures that the system is not holding spinlocks or in an interrupt handler.
 * This helps maintain system stability during VM-related operations.
 */
static void
vm_can_sleep(void)
{
	if (CURCPU_EXISTS()) {
		/* Ensure no spinlocks are held. */
		KASSERT(curcpu->c_spinlocks == 0);

	    /* Ensure not in an interrupt handler. */
		KASSERT(curthread->t_in_interrupt == 0);
	}
}

/**
 * @brief Allocate physical pages.
 * 
 * @param npages Number of pages to allocate.
 * @param kernel Indicates whether the allocation is for the kernel or user.
 * @return paddr_t Physical address of the first page allocated, or 0 on failure.
 */
static paddr_t
getppages(unsigned long npages, struct pt_entry *ptentry)
{
	paddr_t addr;

	addr = frame_table_getppages(npages, kernel);
	if (addr == 0) {
		panic("Out of memory");
	}

	return addr;
}

/**
 * @brief Free allocated physical pages.
 * 
 * @param addr Starting physical address of the pages to free.
 */
static void
freeppages(paddr_t addr){
	frame_table_freeppages(addr);
} 

/**
 * @brief Allocate kernel-space virtual pages.
 * 
 * @param npages Number of pages to allocate.
 * @return vaddr_t Virtual address of the allocated pages, or 0 on failure.
 */
vaddr_t
alloc_kpages(unsigned npages)
{
	paddr_t pa;

	vm_can_sleep();
	pa = getppages(npages, NULL);
	return PADDR_TO_KVADDR(pa);
}

/**
 * @brief Free kernel-space virtual pages.
 * 
 * @param addr Virtual address of the pages to free.
 */
void
free_kpages(vaddr_t addr)
{
	/* get the physical address */
	paddr_t pa = addr - KVADDR_TO_PADDR(addr);
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

	vm_can_sleep();
	/* the user can alloc one page at a time */
	pa = getppages(1, pt_row);
	return pa;
}

/**
 * @brief Frees a user page.
 * 
 * This function releases a physical page that was previously allocated.
 * 
 * @param addr The physical address of the page to free.
 */

void free_upage(paddr_t addr){
	freeppages(addr);
};

/**
 * @brief Handles TLB shootdown requests.
 * 
 * This function is a placeholder for TLB shootdown, which is not implemented
 * in this system. Invoking it results in a panic.
 * 
 * @param ts The TLB shootdown request structure. (Unused)
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

/**
 * @brief Handles virtual memory faults.
 * 
 * This function resolves faults by identifying the fault type, determining the
 * appropriate action (e.g., loading a page, allocating memory, or killing the process),
 * and updating the TLB and page table.
 * 
 * @param faulttype The type of the fault (e.g., read, write, or read-only).
 * @param faultaddress The virtual address that caused the fault.
 * 
 * @return 0 on success, or an error code (e.g., `EFAULT`, `EINVAL`) on failure.
 */

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
		vmstats_hit(VMSTAT_TLB_FAULT);
	#endif

	/* Obtain the first address of the page */
	basefaultaddr = faultaddress & PAGE_FRAME;

	DEBUG(DB_VM, "vm: fault: 0x%x\n", faultaddress);

	switch (faulttype)
	{
 	    case VM_FAULT_READONLY:
			kprintf("vm: got VM_FAULT_READONLY, process killed\n");
			sys__exit(-1);
			return 0;
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
			break;
	    default:
			return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

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
	 *  Verify that the fault address belongs to a valid segment
	 * If as_get_segment_type returns zero, the fault address
	 * does not belong to a valid segment.
	 */
	if(!(seg_type = as_get_segment_type(as, faultaddress))){
		kprintf("vm: got faultaddr out of range, process killed\n");
		sys__exit(-1);
	}
	pt_row = pt_get_entry(as, faultaddress);
	readonly = seg_type == SEGMENT_TEXT;
	switch(pt_row->pt_status)
	{
		case NOT_LOADED:
			/*	alloc a page				*/
			page_paddr = alloc_upage(pt_row);

			/**  
			 * update page table.			
			 * it is important to do it before as_load_page
			 * as the pt_entry will be used to retrieve the
			 * physical address of the page
			 */
			pt_set_entry(pt_row,page_paddr,0, (OPT_NOSWAP_RDONLY && readonly) ? IN_MEMORY_RDONLY : IN_MEMORY); 	

			/*	load the page if needed 	*/
			if(seg_type != SEGMENT_STACK && as_check_in_elf(as,faultaddress))
			{
				as_load_page(as,curproc->p_vnode,faultaddress);
			}
	#if OPT_STATS
			else
			{
    			vmstats_hit(VMSTAT_PAGE_FAULT_ZERO);
			}
	#endif
			break;
		case IN_MEMORY_RDONLY:
		case IN_MEMORY:
	#if OPT_STATS
    		vmstats_hit(VMSTAT_TLB_RELOAD);
	#endif
			break;
		case IN_SWAP:
		#if OPT_SWAP
			/*	alloc the page				*/
			page_paddr = alloc_upage(pt_row);

			/*	swap it from the elf file into memory	*/
			swap_in(page_paddr, pt_row->pt_swap_index);

			/* update page table	*/
			pt_set_entry(pt_row,page_paddr,0, (OPT_NOSWAP_RDONLY && readonly) ? IN_MEMORY_RDONLY : IN_MEMORY); 

			#else
				panic("swap not implemented!");
		#endif
			break;
		
		default:
			panic("Cannot resolve fault");
	}

	KASSERT(seg_type != 0);

	/* update tlb	*/
	tlb_insert(basefaultaddr, pt_row->pt_frame_index * PAGE_SIZE, readonly); 

	return 0;
}
#endif /* OPT_RUDEVM */