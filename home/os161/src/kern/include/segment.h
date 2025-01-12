#ifndef _SEGMENT_H_
#define _SEGMENT_H_

#include <types.h>
#include "opt-smartvm.h"

#if OPT_SMARTVM

/**
 * Struct representing a memory segment in the SMARTVM implementation.
 */
struct segment {
    vaddr_t     seg_first_vaddr;    /* First virtual address of the segment */
    vaddr_t     seg_last_vaddr;     /* Last virtual address of the segment */
    size_t      seg_elf_size;       /* Size of the segment as specified in the ELF file */
    off_t       seg_elf_offset;     /* Offset of the segment within the ELF file */
    size_t      seg_npages;         /* Total size of the segment in pages */
};

/**
 * Function to create a new segment object.
 * Returns: A pointer to an initialized segment
 */
struct segment *segment_create(void);

/**
 * Function to define and initialize a segment's properties.
 * Parameters:
 *  - seg: Pointer to the segment to initialize.
 *  - elf_offset: Offset of the segment within the ELF file.
 *  - base_vaddr: Base virtual address of the segment.
 *  - first_vaddr: First virtual address of the segment.
 *  - last_vaddr: Last virtual address of the segment.
 *  - npages: Number of pages occupied by the segment.
 *  - elfsize: Size of the segment as specified in the ELF file.
 */
void segment_define(struct segment *seg, off_t elf_offset, vaddr_t base_vaddr, vaddr_t first_vaddr, vaddr_t last_vaddr, size_t npages, size_t elfsize); 

/**
 * Function to destroy and deallocate a segment object.
 * Parameters:
 *  - seg: Pointer to the segment to destroy.
 */
void segment_destroy(struct segment *seg);

#endif /* OPT_SMARTVM */

#endif /* _SEGMENT_H_ */
