/* 
 * @file swap.h
 * @author F. Palazzi
 *  
 * This header defines the interface for managing the swap file in a virtual memory system.
 * The swap file serves as a secondary storage location for pages that are evicted from physical memory.
 */

#ifndef _SWAPFILE_H_
#define _SWAPFILE_H_

#include <types.h>
#include <bitmap.h>
#include <kern/fcntl.h>
#include <uio.h>
#include <vfs.h>
#include <vm.h>
#include <vnode.h>


/* Size of the swap file in bytes (9 MB) */
#define SWAP_SIZE 9 * 1024 * 1024

/* Name of the swap file */
#define SWAP_NAME "emu0:/SWAPFILE"

/**
 * @brief Initializes the swap file system.
 * 
 * This function sets up the swap file and any necessary data structures 
 * for managing swapping operations. It must be called during system initialization
 * before any swap-in or swap-out operations.
 */
void swap_init(void);

/**
 * @brief Loads a page from the swap file into physical memory.
 * 
 * This function reads a page from the swap file located at the specified 
 * swap index and places it into the given physical memory address.
 * 
 * @param page_paddr Physical address where the page will be loaded.
 * @param swap_index Index in the swap file where the page is stored.
 */
void swap_in(paddr_t page_paddr, unsigned int swap_index);

/**
 * @brief Evicts a page from physical memory to the swap file.
 * 
 * This function writes the contents of a physical memory page into the 
 * swap file and returns the index in the swap file where the page was stored.
 * 
 * @param page_paddr Physical address of the page to be evicted.
 * @return Index in the swap file where the page was stored.
 */
unsigned int swap_out(paddr_t page_paddr);

#endif /* _SWAPFILE_H_ */
