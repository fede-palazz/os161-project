/* 
 * @file swap.c
 * @author F. Palazzi
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

static struct spinlock swaplock = SPINLOCK_INITIALIZER;


/**
 * @brief Initializes the swap subsystem.
 *
 * This function creates or opens the swap file and initializes the bitmap used
 * to track free and used pages in the file. It must be called during system initialization.
 */
void
swap_bootstrap(void)
{
    int err;
    char swapfile_name[] = SWAP_NAME;

    /* Ensure the swap file size is a multiple of the page size */
    KASSERT(SWAP_SIZE % PAGE_SIZE == 0);

    /* Open or create the swap file */
    err = vfs_open(swapfile_name, O_RDWR | O_CREAT | O_TRUNC, 0, &swapfile);
    if (err) {
        panic("Cannot open SWAPFILE");
    }

    /* Create a bitmap to manage swap file pages */
    swapmap = bitmap_create(SWAP_SIZE / PAGE_SIZE);
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
    struct iovec iov;
    struct uio ku;

#if OPT_STATS
    vmstats_hit(VMSTAT_PAGE_FAULT_DISK);
    vmstats_hit(VMSTAT_PAGE_FAULT_SWAP);
#endif

    KASSERT(page_paddr % PAGE_SIZE == 0);
    KASSERT(swap_index < SWAPFILE_NPAGES);
    spinlock_acquire(&swaplock);
    KASSERT(bitmap_isset(swapmap, swap_index));
    spinlock_release(&swaplock);

    swap_offset = swap_index * PAGE_SIZE;

    uio_kinit(&iov, &ku, (void *)PADDR_TO_KVADDR(page_paddr), PAGE_SIZE, swap_offset, UIO_READ);
    err = VOP_READ(swapfile, &ku);
    if (err) {
        panic("Error swapping in\n");
    }
    if (ku.uio_resid != 0) {
		panic("SWAP: short read on page");
	}

    spinlock_acquire(&swaplock);
    bitmap_unmark(swapmap, swap_index);
    spinlock_release(&swaplock);
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
    struct iovec iov;
    struct uio ku;

#if OPT_STATS
    vmstats_hit(VMSTAT_SWAP_WRITE);
#endif

    KASSERT(page_paddr % PAGE_SIZE == 0);

    spinlock_acquire(&swaplock);
    err = bitmap_alloc(swapmap, &swap_index);
    spinlock_release(&swaplock);
    if (err)
    {
        panic("Out of swap space\n");
    }

    swap_offset = swap_index * PAGE_SIZE;

    uio_kinit(&iov, &ku, (void *)PADDR_TO_KVADDR(page_paddr), PAGE_SIZE, swap_offset, UIO_WRITE);
    err = VOP_WRITE(swapfile, &ku);
    if (err)
    {
        panic("Error swapping out\n");
    }

    return swap_index;
}

/**
 * @brief Free a swap slot in the swap file.
 * 
 * Marks the specified swap slot as available for future use. This function 
 * ensures that the swap index provided is valid and safely releases the 
 * corresponding slot.
 * 
 * @param swap_index The index of the swap slot to free.
 */
void
swap_free(unsigned int swap_index) {
    spinlock_acquire(&swaplock);
    bitmap_unmark(swapmap, swap_index);
    spinlock_release(&swaplock);
}

/**
 * @brief Clean up and destroy the swap system.
 * 
 * Releases all resources associated with the swap system, including the 
 * swap file and any associated metadata. After this function is called, 
 * the swap system will no longer be available until reinitialized.
 */
void
swap_destroy(void) {
    vfs_close(swapfile);
    bitmap_destroy(swapmap);
}
