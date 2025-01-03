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

//void bzero(void *vblock, size_t len);

vaddr_t firstFreeAddress;             // First free virtual address; set by start.S
struct spinlock ft_spinlock = SPINLOCK_INITIALIZER;

static int frame_table_findfreeframes(int nPages);

#if OPT_SWAP
static int frame_table_getvictim();
static int frame_table_swapout(int nPages);
static int victim_index = 0;
#endif

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
    paddr_t firstpaddr;  // Physical address of the first free page
    paddr_t lastpaddr;   // Physical address of the last free page
    size_t frame_table_size;      // Total size of the frame table in bytes
    int frame_table_pages;        // Number of pages needed for the frame table
    int kernel_pages;            // Number of pages occupied by the kernel
    int i;

    /* Get the size of RAM. */
    lastpaddr = mainbus_ramsize();

    /*
     * Cap RAM size at 512 MB for simplicity. Larger sizes would require
     * additional mechanisms for memory access beyond kseg0.
     */
    if (lastpaddr > 512 * 1024 * 1024) {
        lastpaddr = 512 * 1024 * 1024;
    }

    /*
     * Determine the first free physical address based on the virtual address
     * stored by start.S and convert it to a physical address.
     */
    firstpaddr = KVADDR_TO_PADDR(firstFreeAddress);

    KASSERT(lastpaddr % PAGE_SIZE == 0);   
    KASSERT(firstpaddr % PAGE_SIZE == 0);
    // Halt the kernel if address is not aligned with the PAGE_SIZE
    nRamFrames = lastpaddr / PAGE_SIZE;

    /* Allocate the frame table at the first free address. */
    frameTable = (struct FrameTableEntry *)firstFreeAddress;

    /*
     * Calculate the size of the frame table and determine the number of pages
     * occupied by the frame table and the kernel.
     */
    frame_table_size = sizeof(struct FrameTableEntry) * nRamFrames;
    frame_table_pages = DIVROUNDUP(frame_table_size, PAGE_SIZE);
    kernel_pages = firstpaddr / PAGE_SIZE;

    /* Initialize all entries in the frame table as free. */
    for (i = 0; i < nRamFrames; i++) {
        frameTable[i].ft_allocsize = 0;
        frameTable[i].ft_used = 0;
        frameTable[i].ft_lock = 0;
        frameTable[i].ft_ptentry = NULL;
    }

    /* Mark the kernel and frame table regions as used. */
    for (i = 0; i < kernel_pages + frame_table_pages; i++) {
        frameTable[i].ft_used = 1;
        frameTable[i].ft_allocsize = 1;
    }
}

/**
 * @brief Find the index of nPages consecutive free pages.
 * 
 * @param nPages
 * @return Index of the first free page.
 */
static int
frame_table_findfreeframes(int nPages)
{
  int end;
  int beginning;
  end = 0;
  beginning = -1;
  while (end < nRamFrames)
  {
    if (frameTable[end].ft_used == 1)
    {
      beginning = -1;
      end += frameTable[end].ft_allocsize;
    }
    else // Frame is free
    {
      if (beginning == -1)
      {
        beginning = end;
      }
      end++;
      if (end - beginning == nPages)
        break;
    }
  }
  if (end - beginning != nPages)
    beginning = -1;
  return beginning;
}

#if OPT_SWAP
/**
 * @brief Find a swappable victim.
 * 
 * @return Index of the swappable page, -1 if not found.
 */
static int
frame_table_getvictim()
{
  int i;
  for(i=0; i<nRamFrames; i++)
  {
    victim_index = (victim_index + 1) % nRamFrames;
    /* Swap out only user pages */
    if(frameTable[victim_index].ft_ptentry != NULL && !frameTable[victim_index].ft_lock)
    {
      KASSERT(frameTable[victim_index].ft_used == 1);
      KASSERT(frameTable[victim_index].ft_allocsize == 1);
      return victim_index;
    }
  }
  return -1;
}


/**
 * @brief Swap out pages from memory.
 * 
 * @param nPages
 * @return Index of the swapped out frame.
 */
static int
frame_table_swapout(int nPages)
{
  int victim_index;
  int swap_index;
  if(nPages > 1)
  {
    panic("Cannot swap out multiple pages");
  }
  victim_index = frame_table_getvictim(nPages);
  if(victim_index == -1)
  {
    panic("Cannot find swappable victim");
  }
#if OPT_NOSWAP_RDONLY
  if(frameTable[victim_index].ft_ptentry->pt_status == IN_MEMORY_RDONLY){
    pt_set_entry(frameTable[victim_index].ft_ptentry,0,0,NOT_LOADED);
    tlb_remove_by_paddr(victim_index * PAGE_SIZE);
    return victim_index;
  }
#endif //OPT_NOSWAP_RDONLY

  /**  
   * Protect the frame table entry while is swapping out,
   * as the ft_lock = 1 prevent the frame to be selected
   * as a victim for another concurrent swap out.
   */
  frameTable[victim_index].ft_lock = 1;
  spinlock_release(&ft_spinlock);
  swap_index = swap_out(victim_index * PAGE_SIZE);
  spinlock_acquire(&ft_spinlock);
  frameTable[victim_index].ft_lock = 0;
  
  /* Update the page table */
  pt_set_entry(frameTable[victim_index].ft_ptentry,0,swap_index,IN_SWAP);
  tlb_remove_by_paddr(victim_index * PAGE_SIZE);
  return victim_index;
}
#endif //OPT_SWAP


/**
 * @brief Allocates a contiguous block of physical frames.
 * 
 * This function searches for and reserves `nPages` contiguous frames of physical memory.
 * 
 * @param nPages Number of pages to allocate.
 * @param ptentry
 * @return Physical address of the allocated frames, or 0 if no contiguous block is available.
 */
paddr_t
frame_table_getppages(int nPages, struct pt_entry *ptentry) {
    int i;
    int beginning;

    spinlock_acquire(&ft_spinlock);

    beginning = frame_table_findfreeframes(nPages);
    if (beginning == -1)
    {

    #if OPT_SWAP
        beginning = frame_table_swapout(nPages);
        if (beginning == -1)
        {
        spinlock_release(&ft_spinlock);
        return 0;
        }
    #else
        spinlock_release(&ft_spinlock);
        return 0;
    #endif

    }
    bzero((void *)PADDR_TO_KVADDR(beginning * PAGE_SIZE), PAGE_SIZE * nPages);

    frameTable[beginning].ft_allocsize = nPages;
    for (i = 0; i < nPages; i++)
    {
        frameTable[beginning + i].ft_used = 1;
        frameTable[beginning + i].ft_ptentry = ptentry;
    }
    spinlock_release(&ft_spinlock);
    return beginning * PAGE_SIZE;
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

    KASSERT(addr % PAGE_SIZE==0);

    long first = addr / PAGE_SIZE;  // Index of the first frame in the block
    long allocSize = frameTable[first].ft_allocsize;  // Size of the allocation
    frameTable[first].ft_allocsize=0;

    /* Ensure the frame index is within bounds and the allocation size is valid. */
    KASSERT(nRamFrames > first);
    KASSERT(allocSize > 0);

    spinlock_acquire(&ft_spinlock);
    /* Mark all frames in the block as free. */
    for (i = 0; i < allocSize; i++) {
        KASSERT(frameTable[first+i].ft_used==1);
        frameTable[first+i].ft_used = 0;
    }

    spinlock_release(&ft_spinlock);
}
