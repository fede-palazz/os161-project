#include <pt.h>
#include <proc.h>
#include <addrspace.h>
#include <segments.h>
#include <current.h>
#include <kern/errno.h>
#include <swapfile.h>
#include <vm.h>
#include "opt-swap.h"
#include "opt-noswap_rdonly.h"

/**
 * @file pt.c
 * @brief Implements page table management for the virtual memory system.
 * 
 * The page table is a critical data structure for translating virtual addresses 
 * to physical addresses. Each entry in the page table describes the location 
 * of a corresponding page, which may reside in physical memory, in the swap file, 
 * or not yet be loaded (requiring retrieval from the ELF file). 
 * 
 * The page table only contains entries for pages within text, data, and stack 
 * segments of the address space, which minimizes memory usage. Consequently, 
 * virtual addresses cannot be directly mapped to page table indices without 
 * segment-specific logic.
 */

/**
 * @brief Computes the page table index for a given virtual address.
 * 
 * Determines the segment to which the virtual address belongs and calculates 
 * the page table index based on the segment's base address and the number of 
 * pages in preceding segments.
 * 
 * @param as The address space structure.
 * @param vaddr The virtual address for which the page table index is needed.
 * @return int The computed page table index.
 */
static int pt_get_index(struct addrspace *as, vaddr_t vaddr) {
    unsigned int pt_index;

    KASSERT(as != NULL);

    // Determine the segment type to which the virtual address belongs
    switch (as_get_segment_type(as, vaddr)) {
        case SEGMENT_TEXT:
            // Calculate the index relative to the text segment's base address
            pt_index = (vaddr - (as->as_text->seg_first_vaddr & PAGE_FRAME)) / PAGE_SIZE;
            KASSERT(pt_index < as->as_text->seg_npages); // Ensure the index is valid
            return pt_index;

        case SEGMENT_DATA:
            // Calculate the index relative to the data segment, including text pages
            pt_index = as->as_text->seg_npages + 
                       (vaddr - (as->as_data->seg_first_vaddr & PAGE_FRAME)) / PAGE_SIZE;
            KASSERT(pt_index < as->as_text->seg_npages + as->as_data->seg_npages);
            return pt_index;

        case SEGMENT_STACK:
            // Calculate the index relative to the stack segment, including text and data pages
            pt_index = as->as_data->seg_npages + 
                       as->as_text->seg_npages + 
                       (vaddr - (as->as_stack->seg_first_vaddr & PAGE_FRAME)) / PAGE_SIZE;
            KASSERT(pt_index < as->as_data->seg_npages + 
                    as->as_text->seg_npages + 
                    as->as_stack->seg_npages);
            return pt_index;

        default:
            panic("Invalid segment type in pt_get_index!");
    }

    return 0; // This should never be reached
}

/**
 * @brief Allocates and initializes the page table.
 * 
 * Creates a page table with the specified number of entries and initializes 
 * each entry with default values (not loaded, no associated physical frame 
 * or swap index).
 * 
 * @param size The number of entries in the page table.
 * @return struct pt_entry* Pointer to the allocated page table, or NULL on failure.
 */
struct pt_entry *pt_create(unsigned long size) {
    unsigned long i = 0;

    // Allocate memory for the page table
    struct pt_entry *pt = kmalloc(sizeof(struct pt_entry) * size);
    if (pt == NULL) {
        return NULL; // Return NULL if memory allocation fails
    }

    // Initialize each page table entry to default values
    for (i = 0; i < size; i++) {
        pt[i].pt_frame_index = 0;    // No associated physical frame
        pt[i].pt_swap_index = 0;     // No associated swap file index
        pt[i].pt_status = NOT_LOADED; // Mark as not loaded
    }

    return pt;
}

/**
 * @brief Retrieves the page table entry for a given virtual address.
 * 
 * Uses the page table index computation to locate the corresponding page 
 * table entry for the specified virtual address.
 * 
 * @param as The address space structure.
 * @param vaddr The virtual address for which the page table entry is needed.
 * @return struct pt_entry* Pointer to the corresponding page table entry.
 */
struct pt_entry *pt_get_entry(struct addrspace *as, const vaddr_t vaddr) {
    KASSERT(as != NULL);

    // Compute the page table index for the given virtual address
    int pt_index = pt_get_index(as, vaddr);

    // Return the pointer to the page table entry
    return &as->as_ptable[pt_index];
}

/**
 * @brief Deallocates the page table.
 * 
 * Frees the memory allocated for the page table. Ensure that `pt_empty` 
 * has been called beforehand to release all associated physical and 
 * swap resources.
 * 
 * @param entry Pointer to the page table.
 */
void pt_destroy(struct pt_entry *entry) {
    KASSERT(entry != NULL);
    kfree(entry); // Free the memory allocated for the page table
}

/**
 * @brief Frees all pages in memory and the swap file associated with the page table.
 * 
 * Iterates through the page table and deallocates resources (physical memory 
 * or swap space) based on the status of each page table entry.
 * 
 * @param pt Pointer to the page table.
 * @param size The number of entries in the page table.
 */
void pt_empty(struct pt_entry *pt, int size) {
    paddr_t paddr;

    KASSERT(pt != NULL);
    KASSERT(size > 0);

    // Iterate through each entry in the page table
    for (int i = 0; i < size; i++) {
        switch (pt[i].pt_status) {
        #if OPT_NOSWAP_RDONLY
            case IN_MEMORY_RDONLY: // Handle read-only memory pages
        #endif
            case IN_MEMORY:
                // Free the physical page
                paddr = (pt[i].pt_frame_index) * PAGE_SIZE;
                free_upage(paddr);
                break;

            case IN_SWAP:
                #if OPT_SWAP
                    // Free the swap space associated with this page
                    swap_free(pt[i].pt_swap_index);
                #else
                    panic("Swap pages should not exist!");
                #endif
                break;

            default:
                // No action needed for pages that are not loaded
                break;
        }
    }
}

/**
 * @brief Sets the properties of a page table entry.
 * 
 * Configures a page table entry with the specified physical frame index, 
 * swap index, and status.
 * 
 * @param pt_row Pointer to the page table entry.
 * @param paddr Physical address of the page frame.
 * @param swap_index Index in the swap file.
 * @param status Status of the page (e.g., in memory, in swap, not loaded).
 */
void pt_set_entry(struct pt_entry *pt_row, paddr_t paddr, unsigned int swap_index, unsigned char status) {
    #if OPT_NOSWAP_RDONLY
        // Ensure the status is valid
        KASSERT(status == IN_MEMORY || status == IN_MEMORY_RDONLY || status == IN_SWAP || status == NOT_LOADED);
    #else
        KASSERT(status == IN_MEMORY || status == IN_SWAP || status == NOT_LOADED);
    #endif

    KASSERT(swap_index < 0xfff); // Ensure the swap index is within 12 bits

    // Set the frame index (convert physical address to frame number)
    pt_row->pt_frame_index = paddr / PAGE_SIZE;

    // Set the swap index and status
    pt_row->pt_swap_index = swap_index;
    pt_row->pt_status = status;
}
