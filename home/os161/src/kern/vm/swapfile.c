#include <swapfile.h>
#include <bitmap.h>
#include <kern/fcntl.h>
#include <uio.h>
#include <vfs.h>
#include <synch.h>
#include <vm.h>
#include <vnode.h>
#include "opt-stats.h"
#if OPT_STATS
#include <vmstats.h>
#endif

/** 
 * @file swapfile.c
 * @brief Swap file management module.
 * 
 * This module provides functionality for managing a swap file, including 
 * swapping pages between physical memory and the swap file, tracking swap 
 * space usage, and freeing swap space when no longer needed. It uses a 
 * bitmap to keep track of which pages in the swap file are occupied.
 */

/* Static variables for swap file management */
static struct vnode *swapfile;       /**< Vnode representing the swap file. */
static struct bitmap *swapmap;      /**< Bitmap to track used pages in the swap file. */
static struct spinlock swaplock = SPINLOCK_INITIALIZER; /**< Spinlock for synchronizing access to the swapmap. */

/**
 * @brief Initializes the swap file system.
 * 
 * Opens the swap file and creates the necessary data structures, such as the bitmap 
 * to manage the allocation of swap space. Ensures that the swap file size is a multiple 
 * of the page size.
 */
void swap_bootstrap(void)
{
    int err;
    char swapfile_name[] = SWAPFILE_NAME;

    KASSERT(SWAPFILE_SIZE % PAGE_SIZE == 0); /* Ensure the swap file size aligns with page size. */

    /* Open or create the swap file, truncating its size if it already exists. */
    err = vfs_open(swapfile_name, O_RDWR | O_CREAT | O_TRUNC, 0, &swapfile);
    if (err)
    {
        panic("Cannot open SWAPFILE");
    }

    /* Create a bitmap to track the allocation of pages in the swap file. */
    swapmap = bitmap_create(SWAPFILE_NPAGES);
}

/**
 * @brief Shuts down the swap file system.
 * 
 * Closes the swap file and frees the associated data structures, such as the bitmap.
 */
void swap_destroy(void)
{
    /* Close the swap file vnode. */
    vfs_close(swapfile);
    /* Destroy the bitmap that tracks swap usage. */
    bitmap_destroy(swapmap);
}

/**
 * @brief Swaps a page from the swap file into physical memory.
 * 
 * Reads a page from the swap file at the specified index and loads it 
 * into physical memory at the given physical address.
 * 
 * @param page_paddr The physical address where the page will be loaded.
 * @param swap_index The index in the swap file of the page to load.
 */
void swap_in(paddr_t page_paddr, unsigned int swap_index)
{
    int err;
    off_t swap_offset; /* Offset in the swap file for the page. */
    struct iovec iov;
    struct uio ku;

#if OPT_STATS
    vmstats_hit(VMSTAT_PAGE_FAULT_DISK); /* Track disk page faults. */
    vmstats_hit(VMSTAT_PAGE_FAULT_SWAP); /* Track swap page faults. */
#endif

    KASSERT(page_paddr % PAGE_SIZE == 0); /* Ensure the address aligns with page boundaries. */
    KASSERT(swap_index < SWAPFILE_NPAGES); /* Ensure the index is within range. */

    /* Ensure the swapmap entry is set for this index. */
    spinlock_acquire(&swaplock);
    KASSERT(bitmap_isset(swapmap, swap_index));
    spinlock_release(&swaplock);

    /* Calculate the offset in the swap file for this page. */
    swap_offset = swap_index * PAGE_SIZE;

    /* Set up a UIO for reading from the swap file into physical memory. */
    uio_kinit(&iov, &ku, (void *)PADDR_TO_KVADDR(page_paddr), PAGE_SIZE, swap_offset, UIO_READ);
    err = VOP_READ(swapfile, &ku);
    if (err)
    {
        panic("Error swapping in\n");
    }

    /* Ensure the read operation completed fully. */
    if (ku.uio_resid != 0)
    {
        panic("SWAP: short read on page");
    }

    /* Mark the swap entry as free in the bitmap. */
    spinlock_acquire(&swaplock);
    bitmap_unmark(swapmap, swap_index);
    spinlock_release(&swaplock);
}

/**
 * @brief Swaps a page from physical memory to the swap file.
 * 
 * Writes a page from the specified physical address into the swap file 
 * and returns the index of the page in the swap file.
 * 
 * @param page_paddr The physical address of the page to write.
 * @return The index of the page in the swap file.
 */
unsigned int swap_out(paddr_t page_paddr)
{
    int err;
    unsigned int swap_index; /* Index of the allocated swap slot. */
    off_t swap_offset;
    struct iovec iov;
    struct uio ku;

#if OPT_STATS
    vmstats_hit(VMSTAT_SWAP_WRITE); /* Track swap write operations. */
#endif

    KASSERT(page_paddr % PAGE_SIZE == 0); /* Ensure the address aligns with page boundaries. */

    /* Allocate an index in the swap file using the bitmap. */
    spinlock_acquire(&swaplock);
    err = bitmap_alloc(swapmap, &swap_index);
    spinlock_release(&swaplock);
    if (err)
    {
        panic("Out of swap space\n");
    }

    /* Calculate the offset in the swap file for this page. */
    swap_offset = swap_index * PAGE_SIZE;

    /* Set up a UIO for writing to the swap file from physical memory. */
    uio_kinit(&iov, &ku, (void *)PADDR_TO_KVADDR(page_paddr), PAGE_SIZE, swap_offset, UIO_WRITE);
    err = VOP_WRITE(swapfile, &ku);
    if (err)
    {
        panic("Error swapping out\n");
    }

    /* Return the index of the page in the swap file. */
    return swap_index;
}

/**
 * @brief Frees a page in the swap file.
 * 
 * Marks the specified swap index as free in the bitmap, allowing 
 * the space to be reused. No actual deallocation occurs.
 * 
 * @param swap_index The index of the swap page to free.
 */
void swap_free(unsigned int swap_index)
{
    /* Mark the swap index as free in the bitmap. */
    spinlock_acquire(&swaplock);
    bitmap_unmark(swapmap, swap_index);
    spinlock_release(&swaplock);
}
