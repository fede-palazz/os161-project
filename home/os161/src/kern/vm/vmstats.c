/**
 * @brief Implementation of virtual memory statistics tracking.
 * 
 * This file contains functions to record and print virtual memory statistics,
 * such as TLB faults and page faults, using a spinlock for concurrency control.
 */

#include <vmstats.h>

/**
 * @brief Array to store counts of various virtual memory events.
 */
static int vmstats[10];

/**
 * @brief Spinlock to ensure thread-safe access to the vmstats array.
 */
static struct spinlock vmstats_l = SPINLOCK_INITIALIZER;

/**
 * @brief Array of names corresponding to the tracked virtual memory statistics.
 */
static const char *vmstats_names[] = {
    "TLB Faults",
    "TLB Faults with Free",
    "TLB Faults with Replace",
    "TLB Invalidations",
    "TLB Reloads",
    "Page Faults (Zeroed)",
    "Page Faults (Disk)",
    "Page Faults from ELF",
    "Page Faults from Swapfile",
    "Swapfile Writes"
};

/**
 * @brief Increment the count of a specific virtual memory statistic.
 * 
 * This function increments the count of the statistic specified by the index `stat`.
 * 
 * @param stat The index of the statistic to increment. Must be less than 10.
 * The value of `stat` must be a valid index in the vmstats array.
 */
void vmstats_hit(unsigned int stat)
{
    spinlock_acquire(&vmstats_l);

    KASSERT(stat < 10);
    vmstats[stat]++;

    spinlock_release(&vmstats_l);
}

/**
 * @brief Print the current virtual memory statistics.
 * 
 * This function outputs the names and counts of all tracked virtual memory statistics
 * to the console.
 */
void vmstats_print()
{
    kprintf("---------------------------\n");
    kprintf("VM STATS\n");
    kprintf("---------------------------\n");
    for (int i = 0; i < 10; i++)
    {
        kprintf("%s: %d\n", vmstats_names[i], vmstats[i]);
    }
    kprintf("---------------------------\n");
}
