/**
 * @file pt.h
 * @author Luigi Mogano
 * @brief Page Table (PT) header for OS161 with SMARTVM optimization.
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

#if OPT_SMARTVM
/**
 * @brief Defines the possible states of a page table entry.
 */
#define NOT_LOADED 0  /**< Page not loaded into memory or swap. */
#define IN_MEMORY 1   /**< Page loaded in physical memory. */
#define IN_SWAP 2     /**< Page swapped out to secondary storage. *

/**
 * @brief Macros for checking the status of a page table entry.
 */
#define IS_IN_MEMORY(pt_entry) (pt_entry->status == IN_MEMORY) /** Check if page is in memory. */
#define IS_IN_SWAP(pt_entry)   (pt_entry->status == IN_SWAP)   /** Check if page is in swap. */
#define IS_NOT_LOADED(pt_entry) (pt_entry->status == NOT_LOADED) /** Check if page is not loaded. */

/**
 * @struct pt_entry
 * @brief Represents a single entry in the page table.
 * 
 * frame_index is the index of the physical frame where the page resides (if in memory).
 * swap_index is the index in the swap space where the page is stored (if swapped out).
 * status is the status of the page (NOT_LOADED, IN_MEMORY, or IN_SWAP).
 */
struct pt_entry
{
    unsigned int frame_index : 20; /** Physical frame index. */
    unsigned int swap_index : 12;  /** Swap space index. */
    unsigned char status : 2;      /** Page status. */
};

struct pt_entry *pt_get_entry(struct addrspace *as, const vaddr_t vaddr);
struct pt_entry *pt_create(unsigned long pagetable_size);
int pt_set_entry(struct addrspace *as, vaddr_t vaddr, paddr_t paddr, unsigned int swap_index, unsigned char status);
void pt_destroy(struct pt_entry*);

#endif /* OPT_RUDEVM */

#endif /* _PT_H_ */