/**
 * @file frame_table.c
 * @author A. Squillino
 * @brief Implementation of the frame table for physical memory management.
 * 
 * This file contains the implementation of functions to manage the allocation
 * and deallocation of physical memory frames. It includes the initialization
 * of the frame table during system bootstrap, as well as functions to allocate 
 * and free contiguous blocks of physical frames.
 */

#include <frame_table.h>

vaddr_t firstFreeAddress;             // First free virtual address; set by start.S
static int nRamFrames = 0;            // Total number of physical RAM frames
struct FrameTableEntry* frameTable;   // Pointer to the frame table

/**
 * @brief Initializes the frame table during system bootstrap.
 * 
 * This function sets up the frame table based on the physical memory layout.
 * It identifies reserved regions used by the kernel and marks them as occupied.
 */
void
frame_table_bootstrap() {
    paddr_t firstPhysicalAddr;  // Physical address of the first free page
    paddr_t lastPhysicalAddr;   // Physical address of the last available page
    size_t frameTableSize;      // Total size of the frame table in bytes
    int frameTablePages;        // Number of pages needed for the frame table
    int kernelPages;            // Number of pages occupied by the kernel
    int i;

    /* Get the size of RAM. */
    lastPhysicalAddr = mainbus_ramsize();

    /*
     * Cap RAM size at 512 MB for simplicity. Larger sizes would require
     * additional mechanisms for memory access beyond kseg0.
     */
    if (lastPhysicalAddr > 512 * 1024 * 1024) {
        lastPhysicalAddr = 512 * 1024 * 1024;
    }

    /*
     * Determine the first free physical address based on the virtual address
     * stored by start.S and convert it to a physical address.
     */
    firstPhysicalAddr = firstfree - MIPS_KSEG0;
    KASSERT(lastPhysicalAddr % PAGE_SIZE == 0);   // Halt the kernel if address is not aligned with the PAGE_SIZE
    nRamFrames = lastPhysicalAddr / PAGE_SIZE;

    /* Allocate the frame table at the first free address. */
    frameTable = (struct FrameTableEntry *)firstfree;

    /*
     * Calculate the size of the frame table and determine the number of pages
     * occupied by the frame table and the kernel.
     */
    frameTableSize = sizeof(struct FrameTableEntry) * nRamFrames;
    frameTablePages = (frameTableSize + PAGE_SIZE - 1) / PAGE_SIZE;
    kernelPages = firstPhysicalAddr / PAGE_SIZE;

    /* Initialize all entries in the frame table as free. */
    for (i = 0; i < nRamFrames; i++) {
        frameTable[i].used = 0;
        frameTable[i].kernel = 0;
        frameTable[i].allocSize = 0;
    }

    /* Mark the kernel and frame table regions as used. */
    for (i = 0; i < kernelPages + frameTablePages; i++) {
        frameTable[i].used = 1;
        frameTable[i].kernel = 1;
    }
}

/**
 * @brief Allocates a contiguous block of physical frames.
 * 
 * This function searches for and reserves `nPages` contiguous frames of physical memory.
 * 
 * @param nPages Number of pages to allocate.
 * @param kernel Flag indicating whether the frames are for kernel use (1 = kernel, 0 = user).
 * @return Physical address of the allocated frames, or 0 if no contiguous block is available.
 */
paddr_t
frame_table_getppages(int nPages, int kernel) {
    int end = 0;     // Tracks the end of the current search range
    int start = -1;  // Tracks the start of the current search range
    int i;

    /* Search for a contiguous block of free frames. */
    while (end < nRamFrames) {
        if (frameTable[end].used == 1) {
            start = -1;  // Reset the search range
            end += frameTable[end].allocSize;  // Skip used frames
        } 
        else {
            if (start == -1) {
                start = end;  // Mark the start of a potential block
            }
            end++;

            /* If a block of the required size is found, stop searching. */
            if (end - start == nPages) {
                break;
            }
        }
    }

    /* If no suitable block is found, return 0. */
    if (start == -1 || end - start != nPages) {
        return 0;
    }

    /* Mark the block as allocated in the frame table. */
    frameTable[start].allocSize = nPages;
    for (i = 0; i < nPages; i++) {
        frameTable[start + i].used = 1;
        frameTable[start + i].kernel = kernel;
    }

    /* Return the physical address of the allocated block. */
    return start * PAGE_SIZE;
}

/**
 * @brief Frees previously allocated physical frames.
 * 
 * This function marks the frames in a previously allocated block as free,
 * making them available for future allocations.
 * 
 * @param addr Physical address of the first frame to free.
 */
void
frame_table_freeppages(paddr_t addr) {
    long i;
    long first = addr / PAGE_SIZE;  // Index of the first frame in the block
    long allocSize = frameTable[first].allocSize;  // Size of the allocation

    /* Ensure the frame index is within bounds and the allocation size is valid. */
    KASSERT(nRamFrames > first);
    KASSERT(allocSize != 0);

    /* Mark all frames in the block as free. */
    for (i = first; i < first + allocSize; i++) {
        frameTable[i].used = 0;
    }

    /* Clear the allocation size of the first frame. */
    frameTable[first].allocSize = 0;
}
