#include "kheap.h"
#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"
#include <inc/queue.h>

struct PageChunk_List kheap_page_free_list;

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE KERNEL HEAP:
//==============================================
//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #0 kheap_init [GIVEN]
//Remember to initialize locks (if any)
void kheap_init()
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		initialize_dynamic_allocator(KERNEL_HEAP_START, KERNEL_HEAP_START + DYN_ALLOC_MAX_SIZE);
		set_kheap_strategy(KHP_PLACE_CUSTOMFIT);
		kheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		kheapPageAllocBreak = kheapPageAllocStart;
	}
	//==================================================================================
	//==================================================================================
	LIST_INIT(&kheap_page_free_list);
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = alloc_page(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE), PERM_WRITEABLE, 1);
	if (ret < 0)
		panic("get_page() in kern: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	unmap_frame(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE));
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===================================
// [1] ALLOCATE SPACE IN KERNEL HEAP:
//===================================
static void* kmalloc_page_allocator(unsigned int size)
{
    uint32 pages_needed = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
    struct PageChunk *cur = NULL;
    struct PageChunk *found = NULL;

  	// 1 EXACT fit 
    LIST_FOREACH(cur, &kheap_page_free_list) {
        if (cur->num_of_pages == pages_needed) {
            found = cur;
            break; 
        }
    }

    // 2 WORST fit 
    if (found == NULL) {
		cur = NULL;
        LIST_FOREACH(cur, &kheap_page_free_list) {
            if (cur->num_of_pages > pages_needed) { 
                if (found == NULL || cur->num_of_pages > found->num_of_pages) {
                    found = cur;
                }
            }
        }
    }

    if (found != NULL) {
        void* allocate_from = (void*)found->start;
        if (found->num_of_pages > pages_needed) { // this was worst fit
            found->start += (pages_needed * PAGE_SIZE);
            found->num_of_pages -= pages_needed;
        }
        else {
            LIST_REMOVE(&kheap_page_free_list, found);
            free_block(found);
        }
        
		for (uint32 i = 0; i < pages_needed; i++) {
            uint32 va = (uint32)allocate_from + (i * PAGE_SIZE);
            int ret = alloc_page(ptr_page_directory, va, PERM_WRITEABLE, 1);
            if (ret == E_NO_MEM) 
			    panic("kmalloc_page_allocator: Out of physical memory while mapping a free chunk!");
        }
        return allocate_from;
    }

    // 3 Extend the break 
    if (kheapPageAllocBreak + (pages_needed * PAGE_SIZE) <= KERNEL_HEAP_MAX) {
        void* ptr = (void*) kheapPageAllocBreak;
        for (uint32 i = 0; i < pages_needed; i++) {
            uint32 va = kheapPageAllocBreak + (i * PAGE_SIZE);
            int ret = alloc_page(ptr_page_directory, va, PERM_WRITEABLE, 1);
            if (ret == E_NO_MEM) {
                for (uint32 j = 0; j < i; j++) {
                    unmap_frame(ptr_page_directory, kheapPageAllocBreak + (j * PAGE_SIZE));
                }
                return NULL; // no mem
            }
        }
        kheapPageAllocBreak += (pages_needed * PAGE_SIZE);// move the break fo2
        return ptr;
    }

	// 4
    return NULL;
}
void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #1 kmalloc
	//Your code is here
	//Comment the following line
	if(size == 0 ) {
		return 0;
	}
	if(size <= DYN_ALLOC_MAX_BLOCK_SIZE) { // block allocator
		return alloc_block(size);
	}
	else { // page allocator
		return kmalloc_page_allocator(size);
	}
	//TODO: [PROJECT'25.BONUS#3] FAST PAGE ALLOCATOR
}

//=================================
// [2] FREE SPACE FROM KERNEL HEAP:
//=================================
void kfree(void* virtual_address)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #2 kfree
	//Your code is here
	//Comment the following line
	panic("kfree() is not implemented yet...!!");
}

//=================================
// [3] FIND VA OF GIVEN PA:
//=================================
unsigned int kheap_virtual_address(unsigned int physical_address)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #3 kheap_virtual_address
	//Your code is here
	//Comment the following line
	// panic("kheap_virtual_address() is not implemented yet...!!");
	
	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */	
	struct FrameInfo *ptr_frame_info = to_frame_info(physical_address);
	if(ptr_frame_info->references == 0) 
		return 0;
	
	uint32 virt_base = ptr_frame_info->mapped_address;
	uint32 offset = PGOFF(physical_address);
	uint32 pa = virt_base | offset; 
	return pa;
}

//=================================
// [4] FIND PA OF GIVEN VA:
//=================================
unsigned int kheap_physical_address(unsigned int virtual_address)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #4 kheap_physical_address
	//Your code is here
	//Comment the following line
	// panic("kheap_physical_address() is not implemented yet...!!");

	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
	uint32 pdx = ptr_page_directory[PDX(virtual_address)];
	if(!(pdx & PERM_PRESENT))
		return 0;
	uint32* page_table = (uint32*) STATIC_KERNEL_VIRTUAL_ADDRESS(EXTRACT_ADDRESS(pdx));
	uint32 ptx = page_table[PTX(virtual_address)];
	if(!(ptx & PERM_PRESENT))
		return 0;
	uint32 pa = EXTRACT_ADDRESS(ptx) | PGOFF(virtual_address);
	return pa;
}

//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

extern __inline__ uint32 get_block_size(void *va);

void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - krealloc
	//Your code is here
	//Comment the following line
	panic("krealloc() is not implemented yet...!!");
}
