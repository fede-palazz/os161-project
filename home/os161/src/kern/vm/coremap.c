#include <types.h>
#include <vm.h>
#include <lib.h>
#include <coremap.h>
#include <mainbus.h>
#include <swapfile.h>
#include <pt.h>
#include <vm_tlb.h>
#include <synch.h>
#include "opt-swap.h"
#include "opt-noswap_rdonly.h"

vaddr_t firstfree; // First free virtual address; set by start.S 

struct spinlock cm_spinlock = SPINLOCK_INITIALIZER;

static int        coremap_find_freeframes(int npages);
#if OPT_SWAP
static int        coremap_get_victim();
static int        coremap_swapout(int npages);
static int        victim_index = 0;
#endif
static int        nRamFrames = 0; //Number of ram frames
static struct     coremap_entry *coremap;

/**
 * @brief Initializes the coremap during the very initial phase of the system bootstrap. It replaces ram_bootstrap
 *  
 */
void coremap_bootstrap(){
  paddr_t firstpaddr;   //One past end of first free physical page 
  paddr_t lastpaddr;    // One past end of last free physical page  
  size_t coremap_size;  // Size in number of bytes of coremap               
  int coremap_pages;    // Number of coremap pages                  
  int kernel_pages;     // Number of Kernel pages                   
  int i;

//Get RAM size
  lastpaddr = mainbus_ramsize();

  /*
   * Cap the memory to 512MB because if more, accessing it with kseg0 wouldn't be possible
   */
  if (lastpaddr > 512 * 1024 * 1024)
  {
    lastpaddr = 512 * 1024 * 1024;
  }

  /*
   * Get first free virtual address from where start.S saved it and convert it to physical address.
   */
  firstpaddr = KVADDR_TO_PADDR(firstfree);

  KASSERT(lastpaddr % PAGE_SIZE == 0);
  KASSERT(firstpaddr % PAGE_SIZE == 0);

  nRamFrames = lastpaddr / PAGE_SIZE;


  // Allocate the coremap in the firstfree address.
  coremap = (struct coremap_entry *)firstfree;
  
  // Calculate the size of coremap and kernel in pages
  coremap_size = sizeof(struct coremap_entry) * nRamFrames;
  coremap_pages = DIVROUNDUP(coremap_size, PAGE_SIZE);
  kernel_pages = firstpaddr / PAGE_SIZE;

  //  Initialize the coremap entries. 
  for (i = 0; i < nRamFrames; i++)
  {
    coremap[i].cm_allocsize = 0;
    coremap[i].cm_used = 0;
    coremap[i].cm_lock = 0;
    coremap[i].cm_ptentry = NULL;
  }


  // Mark the initial part of the coremap as used by the kernel and the coremap itself
  for (i = 0; i < kernel_pages + coremap_pages; i++)
  {
    coremap[i].cm_used = 1;
    coremap[i].cm_allocsize = 1;
  }

}

/**
 * @brief find the index of n consecutive free pages. Find the index of a contiguous block of npages free frames in the coremap
 * 
 * @param npages The number of contiguous free pages to find
 * @return The index of the first free page
 */
static int
coremap_find_freeframes(int npages)
{
  int end=0;
  int beginning=-1;

  while (end < nRamFrames)
  {
    //If the frame is used, reset beginning and skip the allocated frames
    if (coremap[end].cm_used == 1)
    {
      beginning = -1;
      end += coremap[end].cm_allocsize;
    }
    else // Frame is free
    {
      //If is the start of a free sequence mark it
      if (beginning == -1)
      {
        beginning = end;
      }
      end++;

      //If enough free frames are found, return the starting index (beginning)
      if (end - beginning == npages)
        break;
    }
  }

  //Case of not enough free frames, return -1
  if (end - beginning != npages)
    beginning = -1;

  return beginning;
}

#if OPT_SWAP
/**
 * @brief Find a swappable victim frame for swapping.
 * 
 * This function looks for a page that is eligible for swapping out, meaning it is a user page and is not locked. Round robin approach
 * 
 * @return The index of the swappable frame or -1 if not found.
 */
static int
coremap_get_victim()
{
  int i;

  for(i=0; i<nRamFrames; i++)
  {
    //Increment victim-index to select the next frame
    victim_index = (victim_index + 1) % nRamFrames;

    // Swap out only user pages and not locked ones 
    if(coremap[victim_index].cm_ptentry != NULL && !coremap[victim_index].cm_lock)
    {
      KASSERT(coremap[victim_index].cm_used == 1);
      KASSERT(coremap[victim_index].cm_allocsize == 1);

      return victim_index;
    }
  }

  //No elegible victim found
  return -1;
}

/**
 * @brief Swap out a page from memory.
 * 
 * @param npages The number of pages to swap out
 * @return Index of the frame swapped out.
 */
static int
coremap_swapout(int npages)
{
  int victim_index;

  //Index where the page is stored in swap space
  int swap_index;

  if(npages > 1)
  {
    panic("Cannot swap out multiple pages");
  }

  victim_index = coremap_get_victim(npages);
  if(victim_index == -1)
  {
    panic("Cannot find swappable victim");
  }

#if OPT_NOSWAP_RDONLY
  //If the page is marked as read-only skip the swap
  if(coremap[victim_index].cm_ptentry->pt_status == IN_MEMORY_RDONLY){
    pt_set_entry(coremap[victim_index].cm_ptentry,0,0,NOT_LOADED);
    tlb_remove_by_paddr(victim_index * PAGE_SIZE);
    return victim_index;
  }
#endif


  //Lock the coremap entry to prevent concurrent swapouts for the same page and perform the swap
  coremap[victim_index].cm_lock = 1;
  spinlock_release(&cm_spinlock);
  swap_index = swap_out(victim_index * PAGE_SIZE);
  spinlock_acquire(&cm_spinlock);
  coremap[victim_index].cm_lock = 0;
  

  // Update the page table to reflect that the page is now in swap space 
  pt_set_entry(coremap[victim_index].cm_ptentry,0,swap_index,IN_SWAP);

  //Remove the page from TLB
  tlb_remove_by_paddr(victim_index * PAGE_SIZE);

  return victim_index;
}
#endif

/**
 * @brief Allocate npages pages in the RAM.
 * 
 * @param npages The number of pages
 * @param ptentry The page table entry
 * @return paddr_t of the pages, 0 if no pages are available.
 */
paddr_t
coremap_getppages(int npages, struct pt_entry *ptentry)
{
  int i;
  int beginning;

  spinlock_acquire(&cm_spinlock);

  //Find contiguous free frames
  beginning = coremap_find_freeframes(npages);
  if (beginning == -1)
  {
#if OPT_SWAP
  //If no free frames are found, swap out pages
    beginning = coremap_swapout(npages);
    if (beginning == -1)
    {
      spinlock_release(&cm_spinlock);
      return 0;
    }
#else
    spinlock_release(&cm_spinlock);
    return 0;
#endif
  }

  //Zero out the allocated pages to clean them
  bzero((void *)PADDR_TO_KVADDR(beginning * PAGE_SIZE), PAGE_SIZE * npages);

  //Mark the allocated pages as used and associate them with the page table entry
  coremap[beginning].cm_allocsize = npages;
  for (i = 0; i < npages; i++)
  {
    coremap[beginning + i].cm_used = 1;
    coremap[beginning + i].cm_ptentry = ptentry;
  }
  spinlock_release(&cm_spinlock);
  return beginning * PAGE_SIZE;
}

/**
 * @brief Free allocated pages starting from given physical addr. Sets the used bit to 0.
 * 
 * @param addr The starting physical address of the pages to free
 */
void coremap_freeppages(paddr_t addr)
{
  long i;
  long first;
  long allocSize;

  //Check that address is page-aligned
  KASSERT(addr % PAGE_SIZE == 0);

  //Compute first frame and allocation size
  first = addr / PAGE_SIZE;
  allocSize = coremap[first].cm_allocsize;

  //Reset allocsize
  coremap[first].cm_allocsize = 0;

  KASSERT(nRamFrames > first);
  KASSERT(allocSize > 0);

  spinlock_acquire(&cm_spinlock);

  //Mark the frames as free in the coremap
  for (i = 0; i < allocSize; i++)
  {
    KASSERT(coremap[first + i].cm_used == 1);
    coremap[first + i].cm_used = 0;
  }
  spinlock_release(&cm_spinlock);
}
