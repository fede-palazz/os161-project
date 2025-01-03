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


/**
 * @brief Represents a segment in a program's address space.
 * 
 * This structure stores information about a segment, such as its offset in 
 * the ELF file, the starting virtual address, and the number of pages it 
 * occupies in memory.
 */
struct segment {
    off_t elf_offset;       /* Offset of the segment in the ELF file    */
    vaddr_t start_vaddr;    /* Starting virtual address of the segment  */
    size_t num_pages;       /* Number of pages occupied by the segment  */
};

/**
 * @brief Creates a new segment structure and initializes it to default values.
 * 
 * Allocates memory for a `struct segment` and sets its fields to default values.
 * 
 * @return A pointer to the newly created segment, or NULL if allocation fails.
 */
struct segment *segment_create(void);

/**
 * @brief Defines the properties of an existing segment.
 * 
 * Initializes a segment with specific values, such as the ELF offset, starting 
 * virtual address, and the number of pages it occupies.
 * 
 * @param seg A pointer to the segment structure to be initialized. Must not be NULL.
 * @param elf_offset The ELF file offset for the segment.
 * @param start_vaddr The starting virtual address of the segment.
 * @param num_pages The number of pages in the segment.
 */
void segment_define(struct segment *seg, off_t elf_offset, vaddr_t start_vaddr, size_t num_pages);

/**
 * @brief Destroys a segment and frees its associated memory.
 * 
 * Deallocates the memory used by a `struct segment` and ensures that all 
 * resources are released.
 * 
 * @param seg A pointer to the segment structure to be destroyed. Must not be NULL.
 */
void segment_destroy(struct segment *seg);

#endif /* _SEGMENT_H_ */
