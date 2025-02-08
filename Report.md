# OS161 Project C1 - Group 10

## Index

[TOC]

## 1 - Introduction

This report describes the work done by Federico Palazzi, Luigi Mogano and Angelo Squillino regarding the project of OS internals part of the System and Device Programming course.
We improved **OS161** by including **Virtual Memory Management with Demand Paging and Swapping** (prof. Cabodi project 1).
We chose to implement a per-process page table and to handle the problems of:

- empty virtual memory area;
- writing operations on read-only pages;
- illegal memory accesses.

## 2 - Work division

We proceed studying in parallel at the beginning and discuss together all the concerns and implementation strategies.

Regarding the development, Luigi took care of the TLB part, while Angelo focused on the coremap part and Federico on the page table one.

Afterwards, we put the pieces together and we checked for errors, given that a change in these functions may have broken other changes made in parallel. When it was possible to work independently, we tried to work in parallel.

The last phase was optimization (that involved improvements in victim choice algorithms, reduction of memory waste, and simplification of frametable and swap functions) and troubleshooting.

## 3 - Address Space

In OS161, the ELF file headers only explicitly defines two segments: the *.text* (read-only) and *.data* (read-write) segments. However, when the program is loaded into memory, the virtual address space also contains the *stack* segment, which is automatically created at runtime. Initially, in OS161, these segments were allocated contiguously, but with the introduction of a page table, this restriction is no longer necessary.

The virtual memory space for a user program is mapped from the address `0x000000` to `0x80000000`, and the page size is equal to 4096 bytes (4KB). This allows a maximum of `0x80000` pages. 

A straightforward page table implementation would include `0x80000` entries, but this approach would waste memory due to the empty unused regions. To solve this problem, the page table is optimized to include entries only for pages that are part of a segment. Consequently, calculating the correct page table index becomes more complex, as `index = vaddr / PAGE_SIZE` no longer applies due to skipped empty regions. This approach introduces some internal fragmentation because the first and last virtual addresses of the segments may not be multiples of the `PAGESIZE`.

### 3.1 - Address Space Structure

The address space structure stores information about the three segments and the page table. To achieve modularity and code readability, we used pointers to data structures instead of embedding all details directly in the address space structure.

```c
struct addrspace {
    struct segment *as_text;
    struct segment *as_data;
    struct segment *as_stack;
    struct pt_entry *as_ptable;
};
```

### 3.2 - Segment Structure

Each segment must store various details for locating it in the virtual address space and for loading pages from the ELF file.

- **seg_first_vaddr**: First virtual address of the segment.
- **seg_last_vaddr**: Last virtual address of the segment.
- **seg_elf_size**: Size of the segment portion within the ELF file.
- **seg_elf_offset**: Offset of the segment in the ELF file.
- **seg_npages**: Number of pages occupied by the segment.

```c
struct segment {
    vaddr_t seg_first_vaddr;
    vaddr_t seg_last_vaddr;
    size_t seg_elf_size;
    off_t seg_elf_offset;
    size_t seg_npages;
};
```

Segments are loaded from the ELF file starting from `seg_first_vaddr` up to `seg_last_vaddr`. In order to load correctly the pages from the ELF file, we also need the `seg_elf_offset` and since it is possible that some portions of the segment may not belong to the ELF file, the `seg_elf_size` has to be stored to know if the page has to be allocated and zero-filled (not in the ELF) or loaded from the ELF file.

Since the number and type of segments is fixed, it is not necessary to store their permissions, they can be inferred from the segment's name inside the address space.

### 3.3 - Page Table Structure

The page table is critical for handling TLB misses, as it maps the faulting virtual address to the corresponding physical address or SWAPFILE frame:

- **frame_index**: Identifies the frame in physical memory (20 bits).
- **swap_index**: Identifies the page in the SWAPFILE (12 bits, since 9 MB SWAPFILE).
- **status**: Indicates the page state (IN_MEMORY, IN_SWAP, NOT_LOADED, or IN_MEMORY_RDONLY).

```c
#define NOT_LOADED 0
#define IN_MEMORY 1
#define IN_SWAP 2
#define IN_MEMORY_RDONLY 3

struct pt_entry {
    unsigned int frame_index : 20;
    unsigned int swap_index : SWAP_INDEX_SIZE;
    unsigned char status : 2;
};
```

Although it initially seemed possible to deduce page status from `frame_index` and `swap_index` (e.g., both being zero for NOT_LOADED), this approach could fail since both indices can legitimately be zero. Adding a dedicated status field ensures correctness and simplifies the code.

### 3.4 - Calculating the Page Table Index from a Virtual Address

To find the page table entry for a given virtual address, the segment to which the address belongs must first be identified. This is done by checking whether the address falls within the boundaries of each segment. If it does not belong to any segment, the access is invalid, and the process should be killed.

```c
int as_get_segment_type(struct addrspace *as, vaddr_t vaddr) {
	KASSERT(as != NULL);

    if (vaddr >= as->as_text->seg_first_vaddr && vaddr < as->as_text->seg_last_vaddr) {
        return SEGMENT_TEXT;
    }
    if (vaddr >= as->as_data->seg_first_vaddr && vaddr < as->as_data->seg_last_vaddr) {
        return SEGMENT_DATA;
    }
    if (vaddr >= as->as_stack->seg_first_vaddr && vaddr < as->as_stack->seg_last_vaddr) {
        return SEGMENT_STACK;
    }
    
    return 0;
}
```

The next step is to compute the offset within the segment that is calculated as the difference between the target virtual address and the **base address** of the segment. The base address is the initial address of the page from which the `first_vaddr` belongs to, calculated as `first_vaddr & PAGE_FRAME`.

Finally, the page table index is computed as the sum of the number of pages in the preceding segments and the page offset within the current segment.

Example: For a virtual address in the data segment, the page table index is calculated by adding the number of pages in the text segment to the page offset in the data segment.

```c
static int pt_get_index(struct addrspace *as, vaddr_t vaddr) {
	unsigned int pt_index;

    KASSERT(as != NULL);
    
    switch (as_get_segment_type(as, vaddr)) {
        case SEGMENT_TEXT:
            pt_index = (vaddr - (as->as_text->seg_first_vaddr & PAGE_FRAME)) / PAGE_SIZE;
            KASSERT(pt_index < as->as_text->seg_npages);
            return pt_index;
        case SEGMENT_DATA:
            pt_index = as->as_text->seg_npages +
                       (vaddr - (as->as_data->seg_first_vaddr & PAGE_FRAME)) / PAGE_SIZE;
            KASSERT(pt_index < as->as_text->seg_npages + as->as_data->seg_npages);
            return pt_index;
        case SEGMENT_STACK:
            pt_index = as->as_text->seg_npages + as->as_data->seg_npages +
                       (vaddr - (as->as_stack->seg_first_vaddr & PAGE_FRAME)) / PAGE_SIZE;
            KASSERT(pt_index < as->as_text->seg_npages + 
                    			as->as_data->seg_npages + 
                   				as->as_stack->seg_npages);
            return pt_index;
        default:
            panic("Invalid segment type!");
    }
    
    return 0;
}
```

## 4 - Paging

The main enhancement we made was the implementation of paging. Memory management using ram.c was replaced by coremap, which is now responsible for all memory bootstrap operations.

### 4.1 - Coremap Structure

```c
struct coremap_entry {
    unsigned char cm_used : 1; 			// Frame usage status
    unsigned long cm_allocsize : 20; 	// Allocation size for contiguous pages
    unsigned char cm_lock : 1; 			// Lock for synchronization
    struct pt_entry *cm_ptentry; 		// Page table entry (NULL for kernel pages)
};
```

Each memory frame in the coremap is represented by an entry that stores essential information. 

To manage memory effectively, the kernel performs a linear search in the coremap to locate free frames, identified by a bit indicating whether the frame is free or occupied (`cm_used`).

While user-space frames are allocated one at a time, kernel memory allocations may involve multiple contiguous pages, managed via `kmalloc` and released using `kfree`. To properly deallocate these contiguous allocations, the size of the allocation must be recorded (`cm_allocsize`).

In our project, a per-process page table is implemented. For efficient swap-out operations, it is critical to know which process a coremap entry belongs to. Instead of storing the process or page table directly, we store a pointer to the relevant page table entry for simplicity and efficiency.

Each coremap entry includes additional information:

1. Whether the frame belongs to the kernel or user-space (inferred by checking if the process’s address space pointer is NULL).
2. A lock to prevent concurrent modifications during operations such as swapping.

In addition, for a 32-bit architecture, each frame index can be represented in _20 bits_ (`frame_index = physical_index >> 12`).

### 4.2 - Page Allocation

Page allocation depends on whether the frame is for kernel or user-space:

- _Kernel Pages_: Allocated contiguously via `kmalloc`, requiring consecutive free frames. Kernel pages are never swapped out.
- _User Pages_: Allocated individually, typically during page faults. A linear search in the coremap identifies a free frame. When a frame is freed, the coremap marks it as unused.

If memory is fully utilized, a victim frame is swapped out using a _round-robin algorithm_. The algorithm starting with the victim index, selects an user-space frame (kernel frames are excluded from swapping). The frame is then moved to the swap file, and the corresponding page table entry (`*cm_ptentry`) is updated with the swap index. After swapping out, the frame is cleared and made available for allocation.

Read-only frames present in an ELF file don't need to be swapped out since they are already present in the secondary memory.

### 4.3 - User Page Allocation Workflow

The process for allocating user pages follows these steps:

1. A page fault occurs.
2. The kernel locates the corresponding page table entry.
3. If the page is not in memory, `alloc_upage` is invoked, passing the page table entry.
4. This function calls `getppages`, which in turn invokes `coremap_getppages`.
5. `coremap_getppages` searches for a free frame or, if none is available, swaps out a frame. If both memory and swap space are exhausted, the kernel triggers a panic (Out of swap space).
6. Finally, `coremap_getppages` updates the coremap with a pointer to the page table entry.

## 5 - On-Demand Page Loading

To enable on-demand page loading, we made two key modifications:

1. The ELF file is kept open throughout the execution of the program;
2. The pre-loading of entire segments during the `load_elf `function was disabled

### 5.1 - Keeping the ELF File Open

To keep the ELF file open, we made a small change in the `runprogram` function. Instead of closing the ELF file using `vfs_close`, the file is left open and a pointer to its vnode is saved in a new field within the proc structure, named `p_vnode`.

```c
int runprogram(char _progname) {
    ...
    #if OPT_SMARTVM // Custom VM implementation
    curproc->p_vnode = v;
    #else
    /* Original OS161 implementation */
    vfs_close(v);
    #endif
    ...
}

struct proc {
    ...
    struct vnode *p_vnode; /* vnode for the process's ELF file */
    ...
};
```

### 5.2 - Modifying load_elf and Segment Setup

In the original OS161, `load_elf` reads the ELF header and iterates through the segments twice: once to define the address space and again to load the entire segment into physical memory via `load_segment`.

We simplified this process by removing the segment-loading functionality from `load_elf`. Instead, the updated `as_define_region` function now handles defining and storing information about each segment.

```c
int as_define_region(struct addrspace *as, vaddr_t first_vaddr, size_t memsize, off_t elf_offset, size_t elfsize)
```

This revised function saves the segment's ELF file offset (`elf_offset` in `seg_elf_offset`) and size in the segment structure. This information is crucial for determining where a segment resides in the ELF file and for loading its pages on demand.

### 5.3 - Page Loading

There are two new functions for loading pages on demand: `as_load_page` and `load_page`. These functions are invoked by the `vm_fault` function whenever a page fault occurs for a page that needs to be loaded from the ELF file.

#### Function "load_page"

The `load_page` function reads a page from the ELF file and writes it to physical memory. Its prototype is:

```c
void load_page(struct vnode *v, off_t offset, paddr_t page_paddr, size_t size);
```

_Parameters_:

- `*v`: Pointer to the vnode of the ELF file.
- `offset`: Offset in the ELF file where the page resides.
- `page_paddr`: Physical memory address where the page will be stored.
- `size`: Number of bytes to read from the ELF.

Not all pages are a full 4KB due to unaligned addresses. For example, the first page of a segment may not start on a page-aligned virtual address that ends with 12 zeros. This requires calculating the exact size of the first and last pages separately. This issue is handled bu the `as_load_page` function.

#### Function "as_load_page"

The `as_load_page` function wraps `load_page` and calculates the correct `offset` and `size` for the page being loaded.

```c
int as_load_page(struct addrspace *as, struct vnode *vnode, vaddr_t faultaddress);
```

_Parameters_:

- `*as`: The address space of the process for which we are loading pages.
- `*vnode`: The pointer to the vnode of the ELF file.
- `faultaddress`: The faultaddress that caused the page fault.

This function considers three cases:

1. _First Page of a Segment_

```c
if ((segment->seg_first_vaddr & PAGE_FRAME) == (faultaddress & PAGE_FRAME)) {
    size = PAGE_SIZE - (segment->seg_first_vaddr & ~PAGE_FRAME) > segment->seg_elf_size
    ? segment->seg_elf_size
    : PAGE_SIZE - (segment->seg_first_vaddr & ~PAGE_FRAME);
    offset = segment->seg_elf_offset;
    target_addr = pt_row->pt_frame_index * PAGE_SIZE + (segment->seg_first_vaddr & ~PAGE_FRAME);
}
```

- **size**: If there is only one page in the segment it is set to `seg_elf_size`, otherwise it will be set to the difference between PAGE_SIZE and the last 12 bits of `seg_first_vaddr`.
- **offset**: Matches the segment's offset in the ELF file.
- **target_addr**: Physical memory address where the frame will be allocated, set to `pt_row->pt_frame_index * PAGE_SIZE`, which is the page-aligned physical address of the memory that has been allocated for this page + the last 12 bits of `seg_first_vaddr`.

2. _Last Page of a Segment_

```c
else if (((segment->seg_first_vaddr + segment->seg_elf_size) & PAGE_FRAME) == (faultaddress & PAGE_FRAME)) {
    size = (segment->seg_first_vaddr + segment->seg_elf_size) & ~PAGE_FRAME;
    offset = segment->seg_elf_offset + (faultaddress & PAGE_FRAME) - segment->seg_first_vaddr;
    target_addr = pt_row->pt_frame_index * PAGE_SIZE;
}
```

- **size**: The size of the last page (`segment->seg_first_vaddr + segment->seg_elf_size`).
- **offset**: `segment->seg_elf_offset`  plus the offset between the segment's first virtual address and the page-aligned virtual address of the faulting page.
- **target_addr**: Physical memory address of the allocated frame.

3. _Middle Pages_

```c
else {
size = PAGE_SIZE;
offset = segment->seg_elf_offset + (faultaddress & PAGE_FRAME) - segment->seg_first_vaddr;
target_addr = pt_row->pt_frame_index * PAGE_SIZE;
}
```

- **size**: Fixed at PAGE_SIZE.
- **offset** and **target_addr** same as before

After computing these values, `as_load_page` invokes `load_page` to load the page from the ELF file into memory, then returns control to `vm_fault`.


## 6 - SWAP

To implement SWAP, the kernel initializes a bootstrap function during startup to manage a dedicated SWAPFILE of fixed size (9MB). This file is opened at boot and remains open for the system's runtime. The SWAP module interfaces with the coremap, virtual memory, and page table mechanisms whenever pages need to be swapped in or out of memory.

The SWAPFILE provides space for `0x900` pages, each equals to 4096 bytes (the default system's page size). This size allows addressing pages with 12 bits. To maintain flexibility, the `SWAP_INDEX_SIZE` constant in swapfile.h can be adjusted if `SWAPFILE_SIZE` is changed.

### 6.1 - SWAP Bitmap Management

To track the allocation of pages in the SWAPFILE, a bitmap is used. Each bit in the bitmap corresponds to a page in the SWAPFILE:

- _1_: The page is occupied.
- _0_: The page is free.

The bitmap is created as follows:

```c
static struct bitmap *swapmap;
swapmap = bitmap_create(SWAPFILE_SIZE / PAGE_SIZE);
```

- _Swapping Out_: 
  To swap out a page, the kernel searches for an empty slot in the bitmap. The contents of a memory frame are then copied to the corresponding SWAPFILE page, and the respective bitmap bit is set to 1.

- _Swapping In_: 
  To swap in a page, the kernel reads from the SWAPFILE page at the given index, loads it into memory, and clears the corresponding bitmap bit by setting it to 0.

### 6.2 - SWAP Optimization: Avoiding Redundant Writes

To improve efficiency, the system avoids unnecessary writes to the SWAPFILE when dealing with _read-only pages_. Since read-only pages are unmodified during execution and already exist in the ELF binary, they don’t need to be swapped out. Instead, these pages can be read directly from the ELF file when needed.

This optimization leverages the on-demand paging mechanism, which keeps the ELF file open throughout the process lifecycle. By reading directly from the ELF file instead of the SWAPFILE, the system achieves the following benefits:

1. _Avoids unnecessary writes_: Reduces time spent swapping out read-only pages.
2. _Saves SWAPFILE space_: Frees up space in the SWAPFILE for writable pages.
3. _Maintains read performance_: Accessing the ELF file is as efficient as accessing the SWAPFILE, as both are stored in secondary memory.

This optimization can be toggled using the `noswap_rdonly` option in the kernel configuration. When enabled, the system checks if a page is read-only before swapping it out. If the page is read-only, it is skipped and later loaded from the ELF file during a page fault.

## 7 - VM fault

The `vm_fault` function is called every time the requested vaddr is not present in the TLB or if an illegal operation like writing on a readonly page is attempted.

The function receives two parameters:

- faulttype - that is the type of page fault received, `VM_FAULT_READONLY` if the system tries to write a page whose dirty bit is not flagged on in the relative TLB entry or `VM_FAULT_WRITE`/`VM_FAULT_READ` for attempted read/write access to a page that is not in TLB.
- the virtual address `vaddr` that caused the page fault.

```c
int vm_fault(int faulttype, vaddr_t faultaddress)
```

### 7.1 - Write on read-only page

When a process tries to write on a read-only page, it triggers the `vm_fault` with code `VM_FAULT_READONLY` and the process is killed by the `sys__exit()` called directly within the kernel.

```c
int vm_fault(int faulttype, vaddr_t faultaddress){
...
switch (faulttype)
	{
 	    case VM_FAULT_READONLY:
			kprintf("vm: got VM_FAULT_READONLY, process killed\n");
			sys__exit(-1);
			return 0;
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
			break;
	    default:
			return EINVAL;
	}
...
}
```

The TLB entry is setted using the `tlb_insert` function and passing the value `1` (that stands for true) to the `ro` (that means read-only) parameter when storing the page in physical memory during the VM fault.

```c
void tlb_insert(vaddr_t vaddr, paddr_t paddr, bool ro);
```

This function inserts a new entry in the TLB for the vaddr-paddr pair and sets up the dirty bit based on the `ro` parameter passed to tlb_insert. It uses a round-robin replacement strategy regarding the TLB entries.

To test this functionality, we use _nosywrite_, located in the testbin folder that attempts a write into the text segment:

```c
int main(void){
    int *invalid_pointer = (int *)0x400000;
    *invalid_pointer = 3;
    return 0;
}
```

Where the address `0x400000` is the base address of the text segment.

### 7.2 - Read/Write type faults

If it is requested a virtual address not present in the TLB, read or write type faults are generated.

The vm_fault function first performs page-aligning of the received faultaddress and then with the address space, retrieves the pointer to the correspondant pt_entry with `pt_get_entry()` and the information regarding the type of segment the faultaddr belongs to with `as_get_segment_type()`.

```c
int vm_fault(int faulttype, vaddr_t faultaddress){
...
basefaultaddr = faultaddress & PAGE_FRAME;

	if(!(seg_type = as_get_segment_type(as, faultaddress))){
		kprintf("vm: got faultaddr out of range, process killed\n");
		sys__exit(-1);
	}
	pt_row = pt_get_entry(as, faultaddress);
	readonly = seg_type == SEGMENT_TEXT;
...
}
```

The `seg_type` information is used to set the value of the `readonly` to 1 if the fault address belongs to the text segment.

Given the value stored in the `pt_status` field of the relative pt_entry, it can happen:

- `case IN_MEMORY`/`IN_MEMORY_RDONLY`: page is in physical memory but its TLB entry has been replaced to make room for other entries -> by obtaining the physical address from the page table, the corresponding entry is added to the TLB .
- `case NOT_LOADED` and `seg_type == SEGMENT_STACK`: Stack pages are originally empty and not read from the ELF file, so the kernel allocates a new page in memory, stores its physical address in the page table and adds the entry to the TLB.
- `case NOT_LOADED` and `seg_type != SEGMENT_STACK`: first allocate physical memory for the page by calling `alloc_upage()` then, while text pages must always be filled with data from the ELF file, the data pages must be checked.
  The function `as_check_in_elf(struct addrspace *as, vaddr_t vaddr)` is called to find out if the page needs to be retrieved from the ELF file. It returns `true` if the passed vaddr belongs to a page stored in the ELF file else returns `false`.
  If returns `true`, the function `as_load_page()` is called and the page is read from ELF and stored in the physical memory address allocated before. At the end, the physical address is saved in the page table and the relative entry is added to the TLB.
- `case IN_SWAP`: page has been moved to the SWAPFILE, thus a frame have to be allocated in memory, filled with the data in the SWAPFILE at the index given in the page table. Then page table is updated removing the index of the swapfile and adding the physical address. At the end the TLB entry is stored in TLB.

If the page belongs to the text segment, the added TLB entry will be read-only.

```c
int vm_fault(int faulttype, vaddr_t faultaddress)
{
...
	switch(pt_row->pt_status)
	{
		case NOT_LOADED:
			/*	alloc a page				*/
			page_paddr = alloc_upage(pt_row);
			pt_set_entry(pt_row,page_paddr,0, readonly ? IN_MEMORY_RDONLY : IN_MEMORY);

			/*	load the page if needed 	*/
			if(seg_type != SEGMENT_STACK && as_check_in_elf(as,faultaddress))
			{
				as_load_page(as,curproc->p_vnode,faultaddress);
			}
			break;
		case IN_MEMORY_RDONLY:
		case IN_MEMORY:
			break;
		case IN_SWAP:
			/*	alloc the page				*/
			page_paddr = alloc_upage(pt_row);
			/*	swap it from the elf file into memory	*/
			swap_in(page_paddr, pt_row->pt_swap_index);
			/* update page table	*/
			pt_set_entry(pt_row,page_paddr,0, readonly ? IN_MEMORY_RDONLY : IN_MEMORY);
		break;
		default:
			panic("Cannot resolve fault");
	}
	KASSERT(seg_type != 0);

	/* update tlb	*/
	tlb_insert(basefaultaddr, pt_row->pt_frame_index * PAGE_SIZE, readonly);
	return 0;
}
```

---

## 8 - Statistics

We kept track of 10 events:

1. TLB Fault: on `vm_fault`;
2. TLB Fault with Free: on `tlb_insert` if not all `NUM_TLB` entries have been used;
3. TLB Fault with Replace: on `tlb_insert` in the opposite case to the previous one;
4. TLB Invalidation: on `tlb_invalidate`, after an `as_activate`;
5. TLB Reload: on `vm_fault` if the page is in memory (state `IN_MEMORY`);
6. Page Fault (Zeroed): whether the address belongs to a never-loaded (`NOT_LOADED`) page of type stack or not present in the ELF file;
7. Page Fault (Disk): the page is either `NOT_LOADED` and saved in the ELF or previously swapped out (`IN_SWAP`);
8. Page Fault from ELF: on `as_load_page` when the page is in the ELF file and has to be loaded in memory;
9. Page Fault from Swapfile: on `swap_in`;
10. Swapfile write: on `swap_out`.

An array of strings containing the name of the event and an array containing the number of times that event occurs are declared in memory.
Each time an event occurs, `vmstats_hit` function increments the occurrence counter of the specific event. A spinlock is used to prevent multiple CPUs from trying to increment the value at the same time.
The function `vmstats_print` shows the results when the operating system shuts down, called in the `shutdown` function of `vm.c`.

## 9 - Configuration

We have created a modular kernel so that the features we implemented can be turned on or off using the following configuration options:

- `syscalls` and `waitpid` options add basic functionalities, it is required to enable them for a proper operation of the kernel.

- `smartvm` enables the new memory management system and other important features such as TLB management, per process page table and demand paging.

- `swap` enables the swap functionality, it is optional but very useful.

- `stats` is also optional and enables the statistics functionality.

- `noswap_rdonly` enables the swap optimization described at point 6.2 of this report.

## 10 - Tests

To test our project we mostly used tests already provided with OS161, but we also created the following ones:

- testbin/hugematmult1
- testbin/hugematmult2 (out of swap space with the current amount of memory)
- testbin/nosywrite to obtain `VM_FAULT_READONLY`

The results of the tests are reported below.

### 10.1 - User programs

| RAM: 512K                 | palin  | huge   | sort   | matmult | hugematmult1 | hugematmult2 | ctest  |
| ------------------------- | ------ | ------ | ------ | ------- | ------------ | ------------ | ------ |
| Execution time            | 15.541 | 39.080 | 20.864 | 7.4875  | 56.832       | -            | 1464.0 |
| TLB Faults                | 13986  | 7458   | 6720   | 4341    | 64464        | -            | 248545 |
| TLB Faults with Free      | 13986  | 7439   | 6578   | 4319    | 64446        | -            | 248530 |
| TLB Faults with Replace   | 0      | 19     | 142    | 22      | 18           | -            | 15     |
| TLB Invalidations         | 7824   | 6697   | 2979   | 1218    | 8771         | -            | 247943 |
| TLB Reloads               | 13981  | 3879   | 5055   | 3533    | 58866        | -            | 123624 |
| Page Faults (Zeroed)      | 1      | 512    | 289    | 380     | 2350         | -            | 257    |
| Page Faults (Disk)        | 4      | 3067   | 1376   | 428     | 3248         | -            | 124664 |
| Page Faults from ELF      | 4      | 58     | 25     | 13      | 78           | -            | 1605   |
| Page Faults from Swapfile | 0      | 3009   | 1351   | 415     | 3170         | -            | 123059 |
| Swapfile Writes           | 0      | 3451   | 1567   | 721     | 5450         | -            | 123242 |

### 10.2 - Kernel tests

| Test name | Passed |
| --------- | ------ |
| at        | True   |
| at2       | True   |
| bt        | True   |
| km1       | True   |
| km2       | True   |
| km3 1000  | True   |

## 11 - Conclusions

This project was a valuable learning experience that strengthened our understanding of OS161. We encountered several design decisions and challenges that required problem-solving and deeper analysis.

It provided a practical perspective on operating system concepts, highlighting complexities not always evident in theory. Debugging was particularly challenging, often requiring significant effort to identify issues. However, each problem we solved improved our skills and reinforced our understanding of system development.