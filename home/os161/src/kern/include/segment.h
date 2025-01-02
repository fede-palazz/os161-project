#ifndef _SEGMENT_H_
#define _SEGMENT_H_

#include <types.h>

/**
 * @struct Segment
 * @brief Represents a segment in a program's address space.
 * 
 * This structure stores information about an ELF file segment,
 * including its offset in the ELF file, starting virtual address,
 * and the number of pages it occupies.
 */
struct segment {
    off_t elf_offset;
    vaddr_t start_vaddr;
    size_t num_pages;
};

/**
 * @brief Creates a new segment structure and initializes it to default values.
 * 
 * Allocates memory for a `struct segment` and sets its fields to default values.
 * 
 * @return A pointer to the newly created segment.
 */
struct segment *segment_create(void);

/**
 * @brief Defines the properties of an existing segment.
 * 
 * Initializes a segment with specific values for ELF offset, starting virtual
 * address, and the number of pages.
 * 
 * @param seg A pointer to the segment structure to be initialized.
 * @param elf_offset The ELF offset for the segment.
 * @param start_vaddr The starting virtual address of the segment.
 * @param num_pages The number of pages in the segment.
 */
void segment_define(struct segment *seg, off_t elf_offset, vaddr_t start_vaddr, size_t num_pages); 

void segment_destroy(struct segment *seg);

#endif 