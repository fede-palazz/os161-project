#include <frameTable.h>

vaddr_t firstFreeAddress; // First free virtual address; set by start.S 


static int nRamFrames = 0; // Number of physical RAM frames 
struct FrameTableEntry *frameTable; //Pointer to the frame table


/**
 * @brief Initializes the frame table during bootstrap
 * 
 */
void FrameTableBootstrap(){

   paddr_t firstPhysicalAddr;  // One past end of first free physical page 
  paddr_t lastPhysicalAddr;  // One past end of last free physical page 
  size_t FrameTableSize; // Size of the frame table in bytes 
  int FrameTablePages;
  int KernelPages;
  int i;

  /* Get size of RAM. */

 lastPhysicalAddr = mainbus_ramsize();

  /*
   *Cap RAM size at 512 MB for simplicity.If we had more,
   * we wouldn't be able to access it all through kseg0 and
   * everything would get a lot more complicated.
   */
  if (lastPhysicalAddr > 512 * 1024 * 1024)
  {
    lastPhysicalAddr = 512 * 1024 * 1024;
  }


  /*
   * Get first free virtual address from where start.S saved it.
   * Convert to physical address.
   */
  firstPhysicalAddr = firstfree - MIPS_KSEG0;
  KASSERT(lastPhysicalAddr % PAGE_SIZE == 0);
  nRamFrames = lastPhysicalAddr / PAGE_SIZE;

// Allocates the frameTable right in the firstfree address. 
  frameTable = (struct coremap_entry *)firstfree;

  /* 

   * Compute the size of frame table and kernel in pages in order to set 
   * the pages right after firstfree as used.
   */
  FrameTableSize = sizeof(struct FrameTableEntry) * nRamFrames;
  FrameTablePages = (FrameTableSize + PAGE_SIZE - 1) / PAGE_SIZE;
  KernelPages = firstPhysicalAddr / PAGE_SIZE;

  /*  Initialize the frame table. */
  for (i = 0; i < nRamFrames; i++)
  {
    frameTable[i].allocSize = 0;
    frameTable[i].used = 0;
    frameTable[i].kernel = 0;
  }

    /* 
   * Set the initial part of the frame table as used by the kernel.
   * It contains the exception handlers, the kernel, the frame table and some padding.
   */
  for (i = 0; i < KernelPages + FrameTablePages; i++)
  {
    frameTable[i].used = 1;
    frameTable[i].kernel = 1;
  }

}

/**
 * @brief get nPages from the RAM.
 * 
 * @param nPages Number of pages to allocate
 * @param isKernel Flag indicating whether the frames are for kernel use
 * @return paddr_t of the pages, 0 if no pages are available.
 */
paddr_t FrameTableGetFreePages(int nPages, int kernel)
{
  int end;
  int start;
  int i;

  end = 0;
  start = -1;

  /* Search for a contiguous block of free frames. */
  while (end < nRamFrames)
  {
    if (frameTable[end].used == 1)
    {
      start = -1;
      end += frameTable[end].used;
    }
    else 
    {
      if (start == -1)
      {
        start = end;
      }
      end++;

      if (end - start == nPages)
        break;
    }
  }

  if (start == -1 || end - start != nPages)
    return 0;

  frameTable[start].allocSize = nPages;
  for (i = 0; i < nPages; i++)
  {
    coremap[start + i].used = 1;
    coremap[start + i].kernel = kernel;
  }
  

  return start * PAGE_SIZE;
}


/**
 * @brief Frees previously allocated physical frames.
 *
 * @param addr Physical address of the first frame to free.
 */
void FrameTableFreePages(paddr_t addr){
  long i;
  long first = addr / PAGE_SIZE;
  long allocSize = frameTable[first].allocSize;


  KASSERT(nRamFrames > first);
  KASSERT(allocSize != 0);
  

  for(i = first; i<allocSize; i++){
    frameTable[i].used = 0;
  }

  frameTable[first].allocSize = 0;

  
}