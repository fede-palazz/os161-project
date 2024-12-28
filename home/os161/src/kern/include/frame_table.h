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

#include <lib.h>
#include <mainbus.h>
#include <types.h>
#include <vm.h>

/**
 * @struct FrameTableEntry
 * @brief Represents a single entry in the frame table.
 *
 * Each frame table entry describes the state of a physical memory frame.
 */
struct FrameTableEntry {
    unsigned int used : 1;       /* Indicates if the frame is in use (1 = used, 0 = free) */
    unsigned int kernel : 1;     /* Indicates if the frame is reserved for kernel (1 = kernel, 0 = user) */
    unsigned int allocSize : 16; /* Indicates the size of the allocation (in pages) starting at this frame */
};

/**
 * @brief Initializes the frame table.
 *
 * This function sets up the frame table during system bootstrap. It allocates
 * and initializes the data structures needed to track the usage of physical
 * memory frames.
 */
void frame_table_bootstrap(void);

/**
 * @brief Allocates a contiguous set of free physical pages.
 *
 * This function searches the frame table for `nPages` contiguous free frames
 * and marks them as allocated. If `kernel` is set to 1, the allocation is
 * marked for kernel use.
 *
 * @param nPages The number of contiguous pages to allocate.
 * @param kernel A flag indicating whether the pages are for kernel (1) or user (0) use.
 * @return The physical address of the first allocated frame, or 0 if no suitable block is available.
 */
paddr_t frame_table_getppages(int nPages, int kernel);

/**
 * @brief Frees previously allocated physical pages.
 *
 * This function releases the frames starting at the given physical address,
 * allowing them to be reused. It updates the frame table to reflect the change.
 *
 * @param addr The physical address of the first frame to be freed.
 */
void frame_table_freeppages(paddr_t addr);

#endif
