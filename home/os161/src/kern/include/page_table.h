/**
 * @file page_table.h
 * @brief Header file for page table management.
 *
 * This file defines the structure and functions for managing page tables
 * in the virtual memory system. It provides declarations for creating
 * page tables and retrieving page table entries based on virtual addresses.
 */

#ifndef _PT_H_
#define _PT_H_

#include <proc.h>
#include <addrspace.h>
#include <current.h>
#include <kern/errno.h>
#include <segment.h>


/**
 * @brief Represents an entry in the page table.
 *
 * Each page table entry contains:
 * - `frame_index`: The index of the physical frame in memory, used to locate
 *   the physical page corresponding to the virtual address.
 * - `swap_index`: The index of the swap location, used when the page is
 *   swapped out of physical memory.
 */
struct pt_entry {
    unsigned int frame_index : 20;  /* Index of the physical frame in memory (20 bits)  */
    unsigned int swap_index : 12;   /* Index of the swap location (12 bits)             */
};

/**
 * @brief Retrieves the page table entry for a given virtual address.
 *
 * This function determines which page table entry corresponds to the
 * provided virtual address in the current process's address space.
 *
 * @param vaddr Virtual address whose page table entry is needed.
 * @return Pointer to the page table entry, or NULL if the address is invalid.
 */
struct pt_entry *pt_get_entry(const vaddr_t vaddr);

/**
 * @brief Creates a new page table.
 *
 * Allocates and initializes a new page table with the specified number
 * of entries. Each entry is set to default values (both frame_index
 * and swap_index set to 0).
 *
 * @param pt_size The number of entries in the page table.
 * @return Pointer to the newly created page table, or NULL if allocation fails.
 */
struct pt_entry *pt_create(unsigned long pt_size);

#endif /* _PT_H_ */
