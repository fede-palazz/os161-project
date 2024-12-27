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
pt_get_entry(const vaddr_t vaddr)
{
    int pt_index;                      // Index of the page table entry
    struct addrspace *cur_as;          // Current process's address space

    cur_as = curproc->p_addrspace;     // Get the current process's address space

    // Check if the virtual address falls within the text or data region
    if (vaddr >= cur_as->s_text->base_vaddr && 
        vaddr < cur_as->s_data->base_vaddr + cur_as->s_data->npages * PAGE_SIZE)
    {
        pt_index = vaddr / PAGE_SIZE;  // Calculate the page table index
        return &curproc->p_ptable[pt_index]; // Return the corresponding page table entry
    }

    // Check if the virtual address falls within the stack region
    if (vaddr >= cur_as->s_stack->base_vaddr && 
        vaddr < cur_as->s_stack->base_vaddr + cur_as->s_stack->npages * PAGE_SIZE)
    {
        // Calculate the page table index, offset by the sizes of text and data regions
        pt_index = cur_as->s_text->npages + 
                   cur_as->s_data->npages + 
                   (vaddr - cur_as->s_stack->base_vaddr) / PAGE_SIZE;
        return &curproc->p_ptable[pt_index]; // Return the corresponding page table entry
    }

    return NULL; // Return NULL if the virtual address is invalid or unmapped
}
