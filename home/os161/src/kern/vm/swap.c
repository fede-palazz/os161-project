/* 
 * @file swap.c
 * 
 * This file implements the functionality for managing the swap file in a virtual memory system. 
 * The swap file is used to temporarily store pages evicted from physical memory, providing
 * support for systems with limited RAM.
 */

#include <swap.h>

/* Pointer to the vnode representing the swap file */
static struct vnode *swapfile;

/* Bitmap to track free and occupied pages in the swap file */
static struct bitmap *swapmap;

/* Flag indicating if the swap subsystem is active */
static bool swap_active = false;

// TODO: Add a spinlock for thread-safe operations on the swapmap

/**
 * @brief Initializes the swap subsystem.
 *
 * This function creates or opens the swap file and initializes the bitmap used
 * to track free and used pages in the file. It must be called during system initialization.
 */
void
swap_init(void)
{
    int err;
    char swapfile_name[16];

    /* Ensure the swap file size is a multiple of the page size */
    KASSERT(SWAP_SIZE % PAGE_SIZE == 0);

    /* Open or create the swap file */
    strcpy(swapfile_name, SWAP_NAME);
    err = vfs_open(swapfile_name, O_RDWR | O_CREAT, 0, &swapfile);
    if (err) {
        panic("Cannot open SWAPFILE");
    }

    /* Create a bitmap to manage swap file pages */
    swapmap = bitmap_create(SWAP_SIZE / PAGE_SIZE);

    /* Mark the swap subsystem as active */
    swap_active = true;
}

/**
 * @brief Reads a page from the swap file into physical memory.
 *
 * This function retrieves a page stored in the swap file at the specified index 
 * and copies it into the given physical memory address. The corresponding bit 
 * in the swapmap is cleared after the operation.
 *
 * @param page_paddr Physical address to load the page into.
 * @param swap_index Index in the swap file where the page is stored.
 */
void 
swap_in(paddr_t page_paddr, unsigned int swap_index)
{
    int err;
    off_t swap_offset;
    struct iovec page_iovec;       // Represents the memory region for the page
    struct uio page_uio;           // Represents the I/O operation context

    /* Ensure swap subsystem is active and inputs are valid */
    KASSERT(swap_active);
    KASSERT(page_paddr % PAGE_SIZE == 0);               // Page-aligned physical address
    KASSERT(swap_index < SWAP_SIZE / PAGE_SIZE);    // Valid index
    KASSERT(bitmap_isset(swapmap, swap_index));         // Page must exist in swap file

    /* Calculate the offset in the swap file */
    swap_offset = swap_index * PAGE_SIZE;

    /* Perform a read from the swap file into the physical address */
    uio_kinit(&page_iovec, &page_uio, (void *)PADDR_TO_KVADDR(page_paddr), PAGE_SIZE, swap_offset, UIO_READ);
    err = VOP_READ(swapfile, &page_uio);
    if (err) {
        panic("Error while swapping in\n");
    }

    /* Mark the swap index as free in the bitmap */
    bitmap_unmark(swapmap, swap_index);
}

/**
 * @brief Evicts a page from physical memory to the swap file.
 *
 * This function writes a page from the specified physical address into the swap file.
 * It allocates a free slot in the swap file using the bitmap, writes the page data to
 * the swap file, and returns the index of the slot where the page was stored.
 *
 * @param page_paddr Physical address of the page to be evicted.
 * @return The index in the swap file where the page was stored.
 */
unsigned int 
swap_out(paddr_t page_paddr)
{
    int err;
    unsigned int swap_index;
    off_t swap_offset;
    struct iovec page_iovec;       // Represents the memory region for the page
    struct uio page_uio;           // Represents the I/O operation context

    /* Ensure swap subsystem is active and inputs are valid */
    KASSERT(swap_active);
    KASSERT(page_paddr % PAGE_SIZE == 0);  // Page-aligned physical address

    /* Allocate a free slot in the swap file bitmap */
    err = bitmap_alloc(swapmap, &swap_index);
    if (err) {
        panic("Out of swap space\n");  // No free space in the swap file
    }

    /* Calculate the offset in the swap file */
    swap_offset = swap_index * PAGE_SIZE;

    /* Write the page data to the swap file */
    uio_kinit(&page_iovec, &page_uio, (void *)PADDR_TO_KVADDR(page_paddr), PAGE_SIZE, swap_offset, UIO_WRITE);
    err = VOP_WRITE(swapfile, &page_uio);
    if (err) {
        panic("Error while swapping out\n");
    }

    /* Return the index where the page was stored */
    return swap_index;
}
