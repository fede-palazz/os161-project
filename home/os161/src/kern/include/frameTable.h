#ifndef _FRAMETABLE_H_
#define _FRAMETABLE_H_

#include <types.h>
#include <vm.h>
#include <lib.h>
#include <mainbus.h>

struct FrameTableEntry
{
    unsigned int used : 1;
    unsigned int kernel : 1;
    unsigned int allocSize : 16;
};



void FrameTableBootstrap(void);
paddr_t FrameTableGetFreePages(int nPages, int kernel);
void FrameTableFreePages(paddr_t addr);
#endif