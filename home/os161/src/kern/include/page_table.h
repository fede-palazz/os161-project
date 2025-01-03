/**
 * @file page_table.h
 * @author F. Palazzi
 * 
 * This file defines the structures, macros, and functions for the page table 
 * used in memory management in OS161 with SMARTVM enabled. It provides support 
 * for managing page statuses, frame indices, and swap indices.
 */

#ifndef _PT_H_
#define _PT_H_
#include <proc.h>
#include <addrspace.h>
#include <segment.h>
#include <current.h>
#include <kern/errno.h>
#include <types.h>
#include "opt-smartvm.h"
#include <proc.h>
#include <addrspace.h>
#include <segment.h>
#include <current.h>
#include <kern/errno.h>
#include <swap.h>
#include <vm.h>
#include "opt-swap.h"
#include "opt-noswap_rdonly.h"

#if OPT_SMARTVM

/**
 * @brief Defines the possible states of a page table entry.
 * 
 * - `NOT_LOADED`: The page has not been loaded into memory or swap.
 * - `IN_MEMORY`: The page is currently in physical memory.
 * - `IN_SWAP`: The page has been swapped out to secondary storage.
 * - `IN_MEMORY_RDONLY`: The page is in memory and marked as read-only.
 */
#define NOT_LOADED          0  /* Page not loaded into memory or swap */
#define IN_MEMORY           1  /* Page loaded in physical memory */
#define IN_SWAP             2  /* Page swapped out to secondary storage */
#define IN_MEMORY_RDONLY    3  /* Page loaded in physical memory readonly */

/**
 * @struct pt_entry
 * @brief Represents a single entry in the page table.
 * 
 * A page table entry tracks the physical frame or swap location of a page and its status.
 * 
 * - `frame_index`: Index of the physical frame containing the page, if in memory.
 * - `swap_index`: Index of the swap space where the page is stored, if swapped out.
 * - `status`: Current state of the page (e.g., NOT_LOADED, IN_MEMORY).
 * 
 * The size of `swap_index` varies depending on the optional swap feature.
 */
struct pt_entry {
    unsigned int    pt_frame_index : 20;  /**< Physical frame index of the page */
    
    #if OPT_SWAP
        unsigned int    pt_swap_index : SWAP_INDEX_SIZE; /**< Swap index (variable size with swap enabled) */
    #else
        unsigned int    pt_swap_index : 12; /**< Swap index (default size if swap is not enabled) */
    #endif

    unsigned char   pt_status : 2;  /**< Current status of the page (e.g., IN_MEMORY) */
};

/**
 * @brief Retrieve the page table entry corresponding to a virtual address.
 * 
 * @param as Pointer to the address space to search.
 * @param vaddr Virtual address to look up in the page table.
 * @return A pointer to the page table entry, or NULL if not found.
 */
struct pt_entry *pt_get_entry(struct addrspace *as, const vaddr_t vaddr);

/**
 * @brief Create and initialize a new page table.
 * 
 * Allocates memory for a page table and sets all entries to the default state.
 * 
 * @param pagetable_size Number of entries in the page table.
 * @return A pointer to the newly created page table.
 */
struct pt_entry *pt_create(unsigned long pagetable_size);

/**
 * @brief Clear all entries in the page table.
 * 
 * Resets the page table entries to their default state. This is used to
 * prepare the page table for reuse.
 * 
 * @param pt Pointer to the page table to clear.
 * @param size Number of entries in the page table.
 */
void pt_empty(struct pt_entry *pt, int size);

/**
 * @brief Destroy a page table and free its resources.
 * 
 * Frees the memory allocated for the page table.
 * 
 * @param pt Pointer to the page table to destroy.
 */
void pt_destroy(struct pt_entry *pt);

/**
 * @brief Update a page table entry with specified values.
 * 
 * Modifies an existing page table entry to set its physical address, 
 * swap index, and status.
 * 
 * @param pt_row Pointer to the page table entry to update.
 * @param paddr Physical address of the page.
 * @param swap_index Index in the swap space (if applicable).
 * @param status New status of the page.
 */
void pt_set_entry(struct pt_entry *pt_row, paddr_t paddr, unsigned int swap_index, unsigned char status);

#endif /* OPT_SMARTVM */

#endif /* _PT_H_ */
