/**
 * @file segment.c
 * @author A. Squillino
 * @brief Implementation of segment management in a program's address space.
 * 
 * This file provides the implementation of functions for creating, defining, 
 * and destroying segments. Segments represent contiguous regions of memory 
 * loaded from an ELF file, and this module helps manage their properties and 
 * lifecycle.
 */

#include <segment.h>

/**
 * @brief Create and initialize a new segment structure.
 * 
 * Allocates memory for a `struct segment`, sets its fields to default values 
 * (zeros), and ensures that the allocation is successful.
 * 
 * @return A pointer to the newly created segment structure.
 */
struct segment *segment_create(void) {
    struct segment *seg = kmalloc(sizeof(struct segment));

    KASSERT(seg != NULL);  /* Ensure that the allocation was successful */

    // Initialize fields to default values.
    seg->elf_offset = 0;
    seg->start_vaddr = 0;
    seg->num_pages = 0;

    return seg;
}

/**
 * @brief Define the properties of an existing segment.
 * 
 * Sets the ELF offset, starting virtual address, and number of pages for a 
 * segment. Ensures that the segment is in a valid, uninitialized state before 
 * assigning the new values.
 * 
 * @param seg A pointer to the segment to be defined. Must not be NULL.
 * @param elf_offset The ELF file offset for the segment.
 * @param start_vaddr The starting virtual address of the segment.
 * @param num_pages The number of pages occupied by the segment. 
 */
void segment_define(struct segment *seg, off_t elf_offset, vaddr_t start_vaddr, size_t num_pages) {
    KASSERT(seg != NULL);                 /* Ensure the segment pointer is valid */
    KASSERT(seg->elf_offset == 0);        /* Ensure the segment is not already defined */
    KASSERT(seg->start_vaddr == 0);
    KASSERT(seg->num_pages == 0);

    // Assign properties to the segment.
    seg->elf_offset = elf_offset;
    seg->start_vaddr = start_vaddr;
    seg->num_pages = num_pages;
}

/**
 * @brief Destroy a segment and free its memory.
 * 
 * Deallocates the memory used by a `struct segment` and ensures that the 
 * pointer is valid before doing so.
 * 
 * @param seg A pointer to the segment to be destroyed. Must not be NULL. 
 */
void segment_destroy(struct segment *seg) {
    KASSERT(seg != NULL);  /* Ensure the segment pointer is valid */

    // Free the allocated memory.
    kfree(seg);
}
