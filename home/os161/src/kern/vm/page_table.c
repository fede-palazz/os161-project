/**
 * @file page_table.c
 * @author F. Palazzi
 * @brief Implementation of page table management for virtual memory.
 *
 * This file contains functions for creating and managing page tables in
 * a virtual memory system. The page table maps virtual addresses to
 * physical memory or swap space, and supports operations such as:
 * - Allocating and initializing a new page table.
 * - Retrieving page table entries corresponding to specific virtual addresses.
 */

#include <page_table.h>

/**
 * @brief Calculate the index of the page table entry for a given virtual address.
 * 
 * @param as Pointer to the address space.
 * @param vaddr The virtual address to locate.
 * 
 * @return The index of the page table entry.
 * @note Panics if the virtual address is out of range.
 */

static int 
pt_get_index(struct addrspace *as, vaddr_t vaddr){
    unsigned int previous_pages = 0;
    unsigned int pt_index;

    KASSERT(as != NULL);
    
    // Check if the address falls within the text segment
    if (vaddr >= as->s_text->base_vaddr && vaddr < as->s_text->base_vaddr + as->s_text->npages * PAGE_SIZE)
    {
        pt_index = ( vaddr - as->s_text->base_vaddr ) / PAGE_SIZE;
        KASSERT(pt_index < as->s_text->npages);
        return pt_index;
    }
    previous_pages += as->s_text->npages;
    
    // Check if the address falls within the data segment
    if (vaddr >= as->s_data->base_vaddr && vaddr < as->s_data->base_vaddr + as->s_data->npages * PAGE_SIZE)
    {
        pt_index = previous_pages + ( vaddr - as->s_data->base_vaddr ) / PAGE_SIZE;
        KASSERT(pt_index <  previous_pages + as->s_data->npages);
        return pt_index;
    }
    previous_pages += as->s_data->npages;
    
    // Check if the address falls within the stack segment
    if (vaddr >= as->s_stack->base_vaddr && vaddr < as->s_stack->base_vaddr + as->s_stack->npages * PAGE_SIZE)
    {
        pt_index = previous_pages + (vaddr - as->s_stack->base_vaddr) / PAGE_SIZE ;
        KASSERT(pt_index <  previous_pages + as->s_stack->npages);
    }

    // If no segment matches, panic
    panic("vaddr out of range?!\n");
    return 0;
}

/**
 * @brief Creates a new page table.
 *
 * This function allocates memory for a page table of the given size and
 * initializes its entries to default values (frame_index and swap_index set to 0).
 *
 * @param pt_size The number of entries in the page table.
 * @return Pointer to the newly created page table, or NULL if memory allocation fails.
 */
struct pt_entry* 
pt_create(unsigned long pt_size)
{
    unsigned long i = 0;

    // Allocate memory for the page table
    struct pt_entry* pt = kmalloc(sizeof(struct pt_entry) * pt_size);

    // Return NULL if allocation fails
    if (pt == NULL) {
        return NULL;
    }

    // Initialize all entries in the page table
    for (i = 0; i < pt_size; i++) {
        pt[i].frame_index = 0;  // Default: no frame assigned
        pt[i].swap_index = 0;   // Default: not swapped
        pt[i].status = NOT_LOADED;
    }

    return pt; // Return the pointer to the created page table
}

/**
 * @brief Retrieves the page table entry for a given virtual address.
 *
 * This function determines which region (text, data, or stack) the provided
 * virtual address belongs to and calculates the corresponding page table entry.
 *
 * @param vaddr Virtual address whose page table entry is needed.
 * @return Pointer to the corresponding page table entry, or NULL if the
 *         address is invalid or does not belong to any valid region.
 */
struct pt_entry*
pt_get_entry(struct addrspace *as, const vaddr_t vaddr)
{
     KASSERT(as != NULL);

    int pt_index = pt_get_index(as, vaddr);
    
    return &as->as_ptable[pt_index];
}

/**
 * @brief Set a page table entry with specified properties.
 * 
 * @param as Pointer to the address space.
 * @param vaddr The virtual address for the entry.
 * @param paddr The physical address to map.
 * @param swap_index The swap index (used for swapped-out pages).
 * @param status The status of the page (NOT_LOADED, IN_SWAP, IN_MEMORY).
 * 
 * @return 1 on success, -1 on failure.
 */

int
pt_set_entry(struct addrspace *as, vaddr_t vaddr, paddr_t paddr, unsigned int swap_index, unsigned char status){
    
    KASSERT(as != NULL);
    KASSERT(status == NOT_LOADED || status == IN_SWAP || status == IN_MEMORY);
    KASSERT(    (paddr == 0 && swap_index != 0 && status == IN_SWAP) 
            ||  (paddr != 0 && swap_index == 0 && status == IN_MEMORY) 
            ||  (paddr == 0 && swap_index == 0 && status == NOT_LOADED) );
    
    struct pt_entry *entry = pt_get_entry(as, vaddr);
    if(entry == NULL){
        return -1;
    }    
   
    entry->frame_index = (unsigned int)(paddr>>12);
    entry->swap_index = swap_index;
    entry->status = status;
 
    return 1; 

}

/**
 * @brief Destroy a page table by deallocating its memory.
 * 
 * @param entry Pointer to the page table to destroy.
 */
void 
pt_destroy(struct pt_entry* entry) 
{
    KASSERT(entry != NULL);
    kfree(entry);
}
