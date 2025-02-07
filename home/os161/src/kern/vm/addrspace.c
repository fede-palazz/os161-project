/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <segment.h>
#include <vm_tlb.h>
#include <pt.h>
#include "opt-stats.h"
#if OPT_STATS
#include <vmstats.h>
#endif


#define VM_STACKPAGES 18  //Number of pages for the stack

#if OPT_SMARTVM

/**
 * @brief Creates a new address space structure.
 * 
 * @return struct addrspace* The newly created address space, or NULL if memory allocation failed.
 */
struct addrspace * as_create(void)
{
	struct addrspace *as;

	//Allocate memory for the addrspace
	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	//Initialize
	as->as_data = NULL;
	as->as_text = NULL;
	as->as_stack = NULL;
	as->as_ptable = NULL;

	return as;
}

//Not implemented
int as_copy(struct addrspace *old, struct addrspace **ret)
{
	(void)old;
	(void)ret;
	panic("as_copy() still not implemented!");

	return 0;
}

/**
 * @brief Invalidates the TLB for the current process if it is a user process.
 * This function ensures the TLB is not using outdated information after switching address spaces.
 */
void
as_activate(void)
{
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
         * Kernel thread does not have an address space; 
		 * leave the previous address space in place.
         */
		return;
	}

	//TLB invalidation for the new address space
	tlb_invalidate();
}


void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/**
 * @brief Deallocates memory for both the pages in memory and swapfile, then frees all data structures associated with the address space.
 * 
 * @param as The address space to be destroyed.
 */
void
as_destroy(struct addrspace *as)
{
	//Ensure validity of as
	KASSERT(as != NULL);
	
	int pt_size = as->as_data->seg_npages + as->as_text->seg_npages + as->as_stack->seg_npages;

	//Page table 
	pt_empty(as->as_ptable, pt_size);
	pt_destroy(as->as_ptable);

	//Destroy segments
	segment_destroy(as->as_text);
	segment_destroy(as->as_data);
	segment_destroy(as->as_stack);

	//Free the memory allocated for the address space structure
	kfree(as);
}


/**
 * @brief Set up a segment at virtual address FIRST_VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * BASE_VADDR + NPAGES*PAGE_SIZE .
 * 
 * @param as The address space of the process
 * @param first_vaddr The starting virtual address of the segment
 * @param memsize The size of the segment expressed in bytes
 * @param elf_offset The offset of the segment in the ELF file
 * @param elfsize Size of the segment in the ELF file
 * @return int
 */
int
as_define_region(struct addrspace *as, vaddr_t first_vaddr, size_t memsize, off_t elf_offset, size_t elfsize)
{
	size_t npages;
	vaddr_t last_vaddr = first_vaddr + memsize;
	vaddr_t base_vaddr;

	//Check that as is not null and the segment has a non-zero size
	KASSERT(as != NULL);
	KASSERT(memsize != 0);

	//Compute the number of pages needed for the segment 
	memsize += first_vaddr & ~(vaddr_t)PAGE_FRAME; //Align to next page bounds
	npages =  DIVROUNDUP(memsize,PAGE_SIZE);	//Round up

	//Compute the address of the first page of the segment
	base_vaddr = first_vaddr & PAGE_FRAME;
	
	//If the text segment hasn't been defined yet, define it
	if (as->as_text == NULL) {
		as->as_text = segment_create();
		segment_define(as->as_text, elf_offset, base_vaddr, first_vaddr, last_vaddr, npages, elfsize);
		return 0;
	}

	//Same as above for the data segment
	if (as->as_data == NULL) {
		as->as_data = segment_create();
		segment_define(as->as_data, elf_offset, base_vaddr, first_vaddr, last_vaddr, npages, elfsize);
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	panic("vm: Warning: too many regions");
	return ENOSYS;
}

/**
 * @brief Initializes the stack segment in the address space
 * 
 * @param as The process's address space
 * @param stackptr The pointer where the initial stack pointer has to be stored
 * @return int 
 */
int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	//Check that as is not null
	KASSERT(as != NULL);

	//Create and define the stack segment
	as->as_stack = segment_create();
	segment_define(as->as_stack, 0, USERSTACK - VM_STACKPAGES * PAGE_SIZE, USERSTACK - VM_STACKPAGES * PAGE_SIZE, USERSTACK, VM_STACKPAGES, 0);
	
	/* Set the initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

/**
 * @brief Set up the page table for the address space, based on the segments
 * 
 * @param as The process's address space
 * @return int 
 */
int
as_define_pt(struct addrspace *as)
{
	//Check that as is not null
	KASSERT(as != NULL);

	/* Create the page table based on the segments loaded previously */
	int npages = as->as_data->seg_npages + as->as_text->seg_npages + as->as_stack->seg_npages;
	as->as_ptable = pt_create(npages);

	return 0;
}

/**
 * @brief Determines the type of segment(text,data or stack) that a given vaddr belongs to.
 * 
 * @param as The process's address space
 * @param vaddr The virtual address to check
 * @return int (Defined in addrspace.h)
 */
int
as_get_segment_type(struct addrspace *as, vaddr_t vaddr)
{
	//Check that as is not null
	KASSERT(as != NULL);
    
	
	if (vaddr >= as->as_text->seg_first_vaddr && vaddr < as->as_text->seg_last_vaddr)
    {
        return SEGMENT_TEXT;
    }

    if (vaddr >= as->as_data->seg_first_vaddr && vaddr < as->as_data->seg_last_vaddr)
    {
        return SEGMENT_DATA;
    }

    if (vaddr >= as->as_stack->seg_first_vaddr && vaddr < as->as_stack->seg_last_vaddr)
    {
        return SEGMENT_STACK;
    }
    
    return 0;
}

/**
 * @brief Retrieve the segment from which the vaddr belongs to.
 * 
 * @param as The process's address space
 * @param vaddr The virtual address
 * @return struct segment* 
 */
static
struct segment *
as_get_segment(struct addrspace *as, vaddr_t vaddr){

	//Check that as is not null
	KASSERT(as != NULL);

	switch(as_get_segment_type(as,vaddr)){
		case SEGMENT_TEXT:
			return as->as_text;
		case SEGMENT_DATA:	
			return as->as_data;
		case SEGMENT_STACK:	
			return as->as_stack;
		default:
			panic("invalid segment type! (as_get_segment)");
	}
    return NULL;
}

/**
 * @brief Check whether the virtual address belongs to a page of the segment which has to be loaded from the ELF file.
 * 
 * @param as The address space.
 * @param vaddr The virtual address.
 * @return bool True if the page has to be loaded from the ELF, false otherwise.
 */
bool as_check_in_elf(struct addrspace *as, vaddr_t vaddr){
	struct segment *seg;

	//Check that as is not null
	KASSERT(as != NULL);

	seg = as_get_segment(as, vaddr);
	if(seg == NULL)
	{
		panic("Cannot retrieve as segment");
	}

	KASSERT(vaddr >= seg->seg_first_vaddr);
	KASSERT(vaddr < seg->seg_last_vaddr);

	if(vaddr < ROUNDUP(seg->seg_first_vaddr + seg->seg_elf_size,PAGE_SIZE)) return true;
	return false;
}

/**
 * @brief Load a page from the elf file to the assigned physical frame. 
 * 
 * It computes:
 * - The size 
 * - The offset within the ELF 
 * - The target address where to store it
 * In order to compute those information, it is needed to differentiate
 * three cases:
 * - The faultaddress belongs to the first page of the segment
 * - The faultaddress belongs to the last page of the segment
 * - The faultaddress belongs to a middle page of the segment
 * 
 * @param as The addrspace
 * @param vnode The vnode of the ELF file
 * @param faultaddress The vaddr of the page fault
 * @return int 
 */
int as_load_page(struct addrspace *as,struct vnode *vnode, vaddr_t faultaddress){
	struct segment *segment;
	struct pt_entry *pt_row;
	off_t offset;	//Offset in the ELF
	size_t size;	//Size of memory to load from ELF
	paddr_t target_addr;

#if OPT_STATS
	vmstats_hit(VMSTAT_PAGE_FAULT_DISK);
	vmstats_hit(VMSTAT_PAGE_FAULT_ELF);
#endif

	//Retrieve the page table entry of the fault address
	pt_row = pt_get_entry(as,faultaddress);

	//Retrieve the segment to which it belongs
	segment = as_get_segment(as,faultaddress);

	// Checkthat the fault address belongs to the segment
	KASSERT(faultaddress < ROUNDUP(segment->seg_first_vaddr + segment->seg_elf_size,PAGE_SIZE));
	KASSERT(faultaddress >= (segment->seg_first_vaddr & PAGE_FRAME));

	if((segment->seg_first_vaddr & PAGE_FRAME )== ( faultaddress & PAGE_FRAME )){

	//CASE 1: faultaddress belongs to the first page of the segment

		/* Determine the size of the portion of the firt page to be loaded from the ELF. It is equal to the number of bytes starting from the first vaddr to the first vaddr of the next page. It can happen that the size of the segment is smaller and so we only need to load this portion */
		size = PAGE_SIZE - ( segment->seg_first_vaddr & ~PAGE_FRAME ) > segment->seg_elf_size ? 
				segment->seg_elf_size :								// In case the elfsize is smaller		
				(PAGE_SIZE - ( segment->seg_first_vaddr & ~PAGE_FRAME )) ;	

		//Set the offset in the ELF file
		offset = segment->seg_elf_offset ;

		//Physical address where the page has to be loaded
		target_addr = pt_row->pt_frame_index * PAGE_SIZE 			// Physycal base address			
					+ ( segment->seg_first_vaddr & ~PAGE_FRAME ) ;	// Offset within the segment 		

	}else if(((segment->seg_first_vaddr + segment->seg_elf_size) & PAGE_FRAME) == ( faultaddress & PAGE_FRAME )){
		//CASE 2: faultaddress belongs to the last page of the segment

		/* Determine the size of the portion of the firt page to be loaded from the ELF. It is equal to the size of the last page within the ELF */	
		size = (segment->seg_first_vaddr + segment->seg_elf_size) & ~PAGE_FRAME ;
		
		/* Calculate the offset within the ELF file for the fault address */
		offset = segment->seg_elf_offset + 							//	Offset within the elf				
				(faultaddress & PAGE_FRAME) -						//  Base address of the faulting page	
				segment->seg_first_vaddr ;							//  First vaddr of the segment	

		//Physical address where the page has to be loaded	
		target_addr = pt_row->pt_frame_index * PAGE_SIZE;			//	Physical addr of the faulting page	

	}else{
		//CASE 3: faultaddress belongs to a middle page of the segment

		//The size to be loaded is a full page
		size = PAGE_SIZE;					

		/* Calculate the offset within the ELF file for the fault address */
		offset = segment->seg_elf_offset + 							//	Offset within the elf				
				(faultaddress & PAGE_FRAME) -						//  Base address of the faulting page	
				segment->seg_first_vaddr ;							//  First vaddr of the segment			

		
		//Physical address where the page has to be loaded
		target_addr = pt_row->pt_frame_index * PAGE_SIZE;			//	Physical addr of the faulting page	

	}

	load_page(vnode,offset,target_addr,size);

	return 0;
}

#endif /* OPT_SMARTVM */
