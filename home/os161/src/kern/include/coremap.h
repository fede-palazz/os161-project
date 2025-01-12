#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <pt.h>
#include "opt-smartvm.h"

#if OPT_SMARTVM


/**
 * @brief Represents a single entry in the coremap.
 * 
 
 */
struct coremap_entry
{
    unsigned char       cm_used : 1;    
    unsigned long       cm_allocsize : 20;      
    unsigned char       cm_lock : 1;
    struct pt_entry     *cm_ptentry;            //Page table entry of the page in this frame, if it si a kernel page it is NULL
};

void        coremap_bootstrap(void);
paddr_t     coremap_getppages(int npages, struct pt_entry *ptentry);
void        coremap_freeppages(paddr_t addr);

#endif /* OPT_SMARTVM */

#endif /* _COREMAP_H_ */
