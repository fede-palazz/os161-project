#ifndef _SWAPFILE_H_
#define _SWAPFILE_H_

#include <types.h>
#include "opt-swap.h"

#if OPT_SWAP

/**
 * @file swapfile.h
 * @brief Header file for swap file management.
 * 
 * This file declares constants, macros, and functions used for managing the swap file.
 * The swap file is used as a backing store for virtual memory pages that are evicted 
 * from physical memory. This module enables swapping pages to and from the swap file 
 * and managing swap space efficiently.
 */

/* Swap file configuration constants */
#define SWAPFILE_SIZE 9 * 1024 * 1024 /* Total size of the swap file in bytes (9 MB) */
#define SWAP_INDEX_SIZE 12 /* Number of bits required to index all pages in the swap file */
#define SWAPFILE_NAME "emu0:/SWAPFILE" /* Name of the swap file on the virtual filesystem */
#define SWAPFILE_NPAGES SWAPFILE_SIZE / PAGE_SIZE /* Total number of pages in the swap file */

/* Function declarations */

/**
 * @brief Initializes the swap file and associated data structures.
 * 
 * This function opens or creates the swap file, ensuring its size is correct, and
 * initializes the bitmap used to manage the allocation of swap pages.
 */
void swap_bootstrap(void);

/**
 * @brief Reads a page from the swap file into physical memory.
 * 
 * Moves the page stored at the specified swap file index to the physical memory 
 * at the given physical address.
 * 
 * @param page_paddr The physical address where the page will be loaded.
 * @param swap_index The index of the page in the swap file.
 */
void swap_in(paddr_t page_paddr, unsigned int swap_index);

/**
 * @brief Writes a page from physical memory to the swap file.
 * 
 * Moves the page from the specified physical address to the swap file and returns 
 * the index where the page is stored in the swap file.
 * 
 * @param page_paddr The physical address of the page to be written to the swap file.
 * @return The index of the page in the swap file.
 */
unsigned int swap_out(paddr_t page_paddr);

/**
 * @brief Frees a page in the swap file.
 * 
 * Marks the page at the specified swap file index as free, allowing it to be reused 
 * for future swaps. This operation does not physically remove the page content.
 * 
 * @param swap_index The index of the page to be freed in the swap file.
 */
void swap_free(unsigned int swap_index);

/**
 * @brief Destroys the swap file system.
 * 
 * Closes the swap file and deallocates the bitmap used for managing swap space. 
 * This function should be called during system shutdown to clean up resources.
 */
void swap_destroy(void);

#endif /* OPT_SWAP */

#endif /* _SWAPFILE_H_ */
