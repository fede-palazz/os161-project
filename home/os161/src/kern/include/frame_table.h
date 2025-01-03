/**
 * @file frame_table.h
 * @author A. Squillino
 * @brief Header file for the frame table, which manages physical memory frames.
 *
 * This file defines the structure and functions used to manage physical memory
 * in the form of a frame table. The frame table keeps track of the allocation
 * and usage of physical memory frames by the operating system.
 */

#ifndef _FRAMETABLE_H_
#define _FRAMETABLE_H_

#define FRAMETABLE_KERNEL 1
#define FRAMETABLE_USER 0

#include <lib.h>
#include <mainbus.h>
#include <types.h>
#include <swap.h>
#include <vm_tlb.h>
#include <synch.h>
#include <vm.h>
#include <page_table.h>
#include "opt-smartvm.h"
#include "opt-swap.h"
#include "opt-noswap_rdonly.h"

#if OPT_SMARTVM

/**
 * @struct FrameTableEntry
 * @brief Represents a single entry in the frame table.
 *
 * Each frame table entry describes the state of a physical memory frame.
 */
struct FrameTableEntry {
    unsigned char ft_used : 1;
    unsigned long ft_allocsize : 20;      
    unsigned char ft_lock : 1;
    struct pt_entry *ft_ptentry;            // Page table entry of the page living in this frame, NULL if kernel page
};

/**
 * @brief Initializes the frame table.
 *
 * This function sets up the frame table during system bootstrap. It allocates
 * and initializes the data structures needed to track the usage of physical
 * memory frames.
 */
void frame_table_bootstrap(void);


paddr_t frame_table_getppages(int nPages, struct pt_entry *ptentry);

/**
 * @brief Frees previously allocated physical pages.
 *
 * This function releases the frames starting at the given physical address,
 * allowing them to be reused. It updates the frame table to reflect the change.
 *
 * @param addr The physical address of the first frame to be freed.
 */
void frame_table_freeppages(paddr_t addr);

#endif // OPT_SMARTVM 

#endif // _FRAMETABLE_H_ 
