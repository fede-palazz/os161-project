#include <segment.h>
#include <lib.h>
#include <vm.h>

/**
 * @brief Allocates and initializes the segment data structure
 * 
 * @return struct segment* 
 */
struct segment *segment_create(void){
    // Allocate memory for a segment and ensure success of allocation
    struct segment *seg = kmalloc(sizeof(struct segment));
    KASSERT(seg != NULL);

    //Initialization
    seg->seg_elf_offset = 0;
    seg->seg_first_vaddr = 0;
    seg->seg_last_vaddr = 0;
    seg->seg_npages = 0;
    seg->seg_elf_size = 0;
    
    return seg;
}

/**
 * @brief Set up the given segment with specified properties
 * 
 * @param seg Pointer to the segment to define.
 * @param elf_offset Offset of the segment within the ELF file.
 * @param base_vaddr Base virtual address of the segment.
 * @param first_vaddr First virtual address of the segment.
 * @param last_vaddr Last virtual address of the segment.
 * @param npages Number of pages allocated to the segment.
 * @param elfsize Size of the segment in the ELF file.
 */
void segment_define(struct segment *seg, off_t elf_offset, vaddr_t base_vaddr, vaddr_t first_vaddr, vaddr_t last_vaddr, size_t npages, size_t elfsize){

    //Sanity checks to ensure that seg has been correctly allocated and initialized    
    KASSERT(seg != NULL);
    KASSERT(seg->seg_elf_offset == 0);
    KASSERT(seg->seg_first_vaddr == 0);
    KASSERT(seg->seg_last_vaddr == 0);
    KASSERT(seg->seg_npages == 0);
    KASSERT(seg->seg_elf_size == 0);

    //Check the addresses are within the user address space
    KASSERT(base_vaddr < USERSPACETOP);
    KASSERT(first_vaddr < USERSPACETOP);
    KASSERT(last_vaddr <= USERSPACETOP);


    //Check if ELF segment size exceeds the memory segment size, if yes adjust the size to fit and log a warning
    if (elfsize > (last_vaddr - first_vaddr)) {
        kprintf("ELF: warning: segment filesize > segment memsize\n");
        elfsize = last_vaddr - first_vaddr;
	}

    //Define fields with provided values
    seg->seg_elf_offset = elf_offset;
    seg->seg_first_vaddr = first_vaddr;
    seg->seg_last_vaddr = last_vaddr;
    seg->seg_npages = npages;
    seg->seg_elf_size = elfsize;
}

/**
 * @brief Deallocates the given segment
 * 
 * @param seg Pointer to the segment to deallocate
 */
void segment_destroy(struct segment *seg){
    
    //Check validity
    KASSERT(seg != NULL);

    kfree(seg);
}