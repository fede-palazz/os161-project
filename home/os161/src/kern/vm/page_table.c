/**
 * @file page_table.c
 * @brief Implementation of page table management for virtual memory.
 *
 * This file provides functions to create, retrieve, modify, and destroy
 * page tables. The page table is a key structure in the virtual memory
 * system, mapping virtual addresses to physical frames or swap space.
 */

#include <page_table.h>

/**
 * @brief Calculate the page table index for a given virtual address.
 *
 * Determines the index of the page table entry corresponding to the
 * specified virtual address based on the segment it belongs to
 * (text, data, or stack).
 *
 * @param as Pointer to the address space.
 * @param vaddr The virtual address to locate.
 * @return The index of the page table entry.
 * 
 * @note Panics if the virtual address is out of range or the segment type
 *       is invalid.
 */
static int pt_get_index(struct addrspace *as, vaddr_t vaddr) {
    unsigned int pt_index;

    KASSERT(as != NULL);

    switch (as_get_segment_type(as, vaddr)) {
        case SEGMENT_TEXT:
            pt_index = (vaddr - (as->as_text->seg_first_vaddr & PAGE_FRAME)) / PAGE_SIZE;
            KASSERT(pt_index < as->as_text->seg_npages);
            return pt_index;

        case SEGMENT_DATA:
            pt_index = as->as_text->seg_npages + 
                       (vaddr - (as->as_data->seg_first_vaddr & PAGE_FRAME)) / PAGE_SIZE;
            KASSERT(pt_index < as->as_text->seg_npages + as->as_data->seg_npages);
            return pt_index;

        case SEGMENT_STACK:
            pt_index = as->as_data->seg_npages + as->as_text->seg_npages + 
                       (vaddr - (as->as_stack->seg_first_vaddr & PAGE_FRAME)) / PAGE_SIZE;
            KASSERT(pt_index < as->as_data->seg_npages + as->as_text->seg_npages + as->as_stack->seg_npages);
            return pt_index;

        default:
            panic("Invalid segment type! (pt_get_index)");
    }

    return 0; // This line should never be reached.
}

/**
 * @brief Create a new page table.
 *
 * Allocates memory for a page table of the given size and initializes
 * all its entries to default values (NOT_LOADED status, frame_index = 0, 
 * swap_index = 0).
 *
 * @param pagetable_size The number of entries in the page table.
 * @return Pointer to the newly created page table, or NULL if memory allocation fails.
 */
struct pt_entry* pt_create(unsigned long pagetable_size) {
    unsigned long i = 0;
    struct pt_entry *pt = kmalloc(sizeof(struct pt_entry) * pagetable_size);

    if (pt == NULL) {
        return NULL;
    }

    for (i = 0; i < pagetable_size; i++) {
        pt[i].pt_frame_index = 0;
        pt[i].pt_swap_index = 0;
        pt[i].pt_status = NOT_LOADED;
    }

    return pt;
}

/**
 * @brief Retrieve the page table entry for a given virtual address.
 *
 * Locates the page table entry corresponding to a specified virtual address
 * by identifying its segment type and calculating the index within the page table.
 *
 * @param as Pointer to the address space.
 * @param vaddr The virtual address to look up.
 * @return Pointer to the corresponding page table entry.
 */
struct pt_entry* pt_get_entry(struct addrspace *as, const vaddr_t vaddr) {
    KASSERT(as != NULL);

    int pt_index = pt_get_index(as, vaddr);
    return &as->as_ptable[pt_index];
}

/**
 * @brief Set a page table entry with specific properties.
 *
 * Updates the fields of a page table entry to map a virtual address
 * to a physical address or swap index, with a specified page status.
 *
 * @param pt_row Pointer to the page table entry to update.
 * @param paddr Physical address of the page (or frame index).
 * @param swap_index Index in the swap space (if applicable).
 * @param status Status of the page (e.g., IN_MEMORY, IN_SWAP, NOT_LOADED).
 */
void pt_set_entry(struct pt_entry *pt_row, paddr_t paddr, unsigned int swap_index, unsigned char status) {
#if OPT_NOSWAP_RDONLY
    KASSERT(status == IN_MEMORY || status == IN_MEMORY_RDONLY || status == IN_SWAP || status == NOT_LOADED);
#else
    KASSERT(status == IN_MEMORY || status == IN_SWAP || status == NOT_LOADED);
#endif
    KASSERT(swap_index < 0xfff); // Ensure swap index fits within 12 bits.

    pt_row->pt_frame_index = paddr / PAGE_SIZE;
    pt_row->pt_swap_index = swap_index;
    pt_row->pt_status = status;
}

/**
 * @brief Destroy a page table.
 *
 * Frees the memory allocated for the page table structure.
 *
 * @param entry Pointer to the page table to destroy.
 */
void pt_destroy(struct pt_entry* entry) {
    KASSERT(entry != NULL);
    kfree(entry);
}

/**
 * @brief Clear a page table and deallocate its pages.
 *
 * Frees up physical pages or swap space associated with each entry
 * in the page table. The table itself is not destroyed.
 *
 * @param pt Pointer to the page table.
 * @param size The number of entries in the page table.
 */
void pt_empty(struct pt_entry* pt, int size) {
    paddr_t paddr;
    KASSERT(pt != NULL);

    for (int i = 0; i < size; i++) {
        switch (pt[i].pt_status) {
            #if OPT_NOSWAP_RDONLY
            case IN_MEMORY_RDONLY:
            #endif
            case IN_MEMORY:
                paddr = (pt[i].pt_frame_index) * PAGE_SIZE;
                free_upage(paddr); // Free the physical page.
                break;

            case IN_SWAP:
                #if OPT_SWAP       
                    swap_free(pt[i].pt_swap_index); // Free the swap space.
                #else           
                    panic("Swap pages should not exist without swap support!");
                #endif
                break;

            default:
                break;
        }
    }
}
