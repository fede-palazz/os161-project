/**
 * @file segment.h
 * @author A. Squillino
 * @brief Interface for managing program segments in the address space.
 * 
 * This file provides the definition of the `struct segment` and associated 
 * functions for handling ELF file segments in a program's address space. 
 * Segments represent contiguous regions of virtual memory, such as text, 
 * data, or stack segments, and are crucial for proper memory management.
 */

#ifndef _SEGMENT_H_
#define _SEGMENT_H_

#include <types.h>
#include <lib.h>
#include <vm.h>
#include "opt-smartvm.h"

#if OPT_SMARTVM

struct segment {
    vaddr_t seg_first_vaddr;    /*  actual first address of the segment         */
    vaddr_t seg_last_vaddr;     /*  last address of the segment                 */
    size_t seg_elf_size;       /*  size of the segment within the elf          */
    off_t seg_elf_offset;     /*  offset of the segment within the elf        */
    size_t seg_npages;         /*  size of the segment in pages                */
};

/**
 * @brief Creates a new segment structure and initializes it to default values.
 * 
 * Allocates memory for a `struct segment` and sets its fields to default values.
 * 
 * @return A pointer to the newly created segment, or NULL if allocation fails.
 */
struct segment *segment_create(void);
void segment_define(struct segment *seg, off_t elf_offset, vaddr_t base_vaddr, vaddr_t first_vaddr, vaddr_t last_vaddr, size_t npages, size_t elfsize); 
void segment_destroy(struct segment *seg);

#endif /* OPT_SMARTVM */

#endif /* _SEGMENT_H_ */
