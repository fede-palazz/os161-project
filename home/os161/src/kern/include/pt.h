#ifndef _PT_H_
#define _PT_H_

#include <types.h>
#include <addrspace.h>
#include "opt-smartvm.h"
#include "opt-noswap_rdonly.h"
#include "opt-swap.h"
#include <swapfile.h>

/**
 * @file pt.h
 * @brief Header file for page table management.
 * 
 * Defines the structure of a page table entry and provides function prototypes 
 * for managing page tables, including creating, destroying, and updating 
 * entries, as well as retrieving specific entries based on virtual addresses.
 * 
 * Page tables are used to translate virtual addresses to physical addresses 
 * and manage the state of pages, including their presence in physical memory, 
 * the swap file, or their status as not yet loaded.
 */

#if OPT_SMARTVM

/** @brief Page status constants. */
#define NOT_LOADED 0       /**< Page is not loaded into memory or swap. */
#define IN_MEMORY 1        /**< Page is currently in physical memory. */
#define IN_SWAP 2          /**< Page is currently stored in the swap file. */
#define IN_MEMORY_RDONLY 3 /**< Page is in memory and marked as read-only (if enabled). */

/**
 * @struct pt_entry
 * @brief Represents a single entry in the page table.
 * 
 * Each entry describes the location and state of a single virtual memory page.
 * 
 * Fields:
 * - `pt_frame_index`: Index of the physical frame in memory, if the page is in memory.
 * - `pt_swap_index`: Index in the swap file, if the page is in swap.
 * - `pt_status`: Status of the page (e.g., in memory, in swap, not loaded).
 */
struct pt_entry {
    unsigned int pt_frame_index : 20; /**< Physical frame index (20 bits). */
#if OPT_SWAP
    unsigned int pt_swap_index : SWAP_INDEX_SIZE; /**< Swap file index, size depends on SWAP_INDEX_SIZE. */
#else
    unsigned int pt_swap_index : 12; /**< Swap file index (12 bits by default). */
#endif
    unsigned char pt_status : 2; /**< Page status (e.g., NOT_LOADED, IN_MEMORY). */
};

/**
 * @brief Retrieves the page table entry for a specific virtual address.
 * 
 * Given a virtual address, this function locates the corresponding 
 * page table entry using the address space structure.
 * 
 * @param as Pointer to the address space structure.
 * @param vaddr The virtual address for which the entry is required.
 * @return Pointer to the page table entry.
 */
struct pt_entry *pt_get_entry(struct addrspace *as, const vaddr_t vaddr);

/**
 * @brief Creates a new page table.
 * 
 * Allocates and initializes a page table with the specified number of entries. 
 * Each entry is initialized to default values (e.g., NOT_LOADED).
 * 
 * @param pagetable_size The number of entries in the page table.
 * @return Pointer to the newly created page table, or NULL on failure.
 */
struct pt_entry *pt_create(unsigned long pagetable_size);

/**
 * @brief Frees all resources associated with a page table.
 * 
 * This function should be called after releasing all associated 
 * memory and swap resources using `pt_empty`. It deallocates the memory 
 * used by the page table itself.
 * 
 * @param pt Pointer to the page table to be destroyed.
 */
void pt_destroy(struct pt_entry *pt);

/**
 * @brief Frees all memory and swap resources associated with a page table.
 * 
 * Iterates through the page table and releases resources (e.g., physical memory 
 * or swap space) for each entry based on its status.
 * 
 * @param pt Pointer to the page table.
 * @param size The number of entries in the page table.
 */
void pt_empty(struct pt_entry *pt, int size);

/**
 * @brief Updates a specific page table entry.
 * 
 * Configures a page table entry with the given physical frame index, 
 * swap index, and status. This function ensures that all parameters are valid.
 * 
 * @param pt_row Pointer to the page table entry to be updated.
 * @param paddr Physical address of the page's frame in memory.
 * @param swap_index Index of the page in the swap file.
 * @param status Status of the page (e.g., IN_MEMORY, IN_SWAP).
 */
void pt_set_entry(struct pt_entry *pt_row, paddr_t paddr, unsigned int swap_index, unsigned char status);

#endif /* OPT_SMARTVM */

#endif /* _PT_H_ */
