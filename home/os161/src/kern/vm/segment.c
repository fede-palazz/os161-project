#include <segment.h>
#include <lib.h>

struct segment *segment_create(void){
    struct segment *seg = kmalloc(sizeof(struct segment));

    KASSERT(seg != NULL);

    seg->elf_offset = 0;
    seg->start_vaddr = 0;
    seg->num_pages = 0;

    return seg;
}


void segment_define(struct segment *seg, off_t elf_offset, vaddr_t start_vaddr, size_t num_pages){

    KASSERT(seg != NULL);

    seg->elf_offset = elf_offset;
    seg->start_vaddr = start_vaddr;
    seg->num_pages = num_pages;
}