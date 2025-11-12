#include "kheap.h"
#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"
#include <inc/queue.h>
#include "kheap_bst.h"

struct PageChunk_List kheap_page_free_list;
// trees
struct PageChunkNode* kheap_free_tree_by_size = NULL;
struct PageChunkNode* kheap_free_tree_by_addr = NULL;
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
	kheap_free_tree_by_size = NULL;
	kheap_free_tree_by_addr = NULL;
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
static void set_allocation_size_info(uint32 start_va, uint32 pages_needed)
{
    uint32* ptr_page_table;
    struct FrameInfo* first_frame_info = get_frame_info(ptr_page_directory, start_va, &ptr_page_table);
    if (first_frame_info != NULL) 
        first_frame_info->num_of_allocated_pages = pages_needed;
    else panic("set_allocation_size_info: first frame is not mapped!");

    for (uint32 i = 1; i < pages_needed; i++) {
        struct FrameInfo* subsequent_frame_info = get_frame_info(ptr_page_directory, start_va + (i * PAGE_SIZE), &ptr_page_table);
        if (subsequent_frame_info != NULL) 
            subsequent_frame_info->num_of_allocated_pages = 0;
    }
}
// FAST KMALLOC USING BST INSERTIONS AND EXACT AND MAX SELECTIONS
static void* page_allocator_fast(unsigned int size)
{
    uint32 pages_needed = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;

	// 1 Exact 
    struct PageChunkNode* found = bst_find_exact_fit(pages_needed);
	
	// 2 Worst
    if (found == NULL) 
	    found = bst_find_worst_fit(pages_needed);
    
    if (found != NULL) {
        void* allocate_from = (void*)found->start;

        bst_remove_by_size(found);
        bst_remove_by_addr(found);

        if (found->num_of_pages > pages_needed) {
            found->start += (pages_needed * PAGE_SIZE);
            found->num_of_pages -= pages_needed;
    
            bst_insert_by_size(found);
            bst_insert_by_addr(found);
        }
        else {
            free_block(found);
        }

        for (uint32 i = 0; i < pages_needed; i++) {
            uint32 va = (uint32)allocate_from + (i * PAGE_SIZE);
            int ret = alloc_page(ptr_page_directory, va, PERM_WRITEABLE, 1);
            if (ret == E_NO_MEM) panic("kmalloc_fast: Out of physical memory!");
        }
        set_allocation_size_info((uint32)allocate_from, pages_needed); // setting allocation here
        return allocate_from;
    }
    
    // 3 Extend the Break
    if (kheapPageAllocBreak + (pages_needed * PAGE_SIZE) <= KERNEL_HEAP_MAX) {
        void* ptr = (void*) kheapPageAllocBreak;
        for (uint32 i = 0; i < pages_needed; i++) {
            uint32 va = kheapPageAllocBreak + (i * PAGE_SIZE);
            int ret = alloc_page(ptr_page_directory, va, PERM_WRITEABLE, 1);
            if (ret == E_NO_MEM) {
                for (uint32 j = 0; j < i; j++) {
                    unmap_frame(ptr_page_directory, kheapPageAllocBreak + (j * PAGE_SIZE));
                }
                return NULL;
            }
        }
        set_allocation_size_info((uint32)kheapPageAllocBreak, pages_needed); // setting allocation 
        kheapPageAllocBreak += (pages_needed * PAGE_SIZE);
        return ptr;
    }
    // 4 
    return NULL;
}

static void* page_allocator(unsigned int size)
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
        if (found->num_of_pages > pages_needed) { 
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
        
        set_allocation_size_info((uint32)allocate_from, pages_needed); 
        return allocate_from;
    }

    if (kheapPageAllocBreak + (pages_needed * PAGE_SIZE) <= KERNEL_HEAP_MAX) {
        void* ptr = (void*) kheapPageAllocBreak;
        for (uint32 i = 0; i < pages_needed; i++) {
            uint32 va = kheapPageAllocBreak + (i * PAGE_SIZE);
            int ret = alloc_page(ptr_page_directory, va, PERM_WRITEABLE, 1);
            
            if (ret == E_NO_MEM) {
                for (uint32 j = 0; j < i; j++) {
                    unmap_frame(ptr_page_directory, kheapPageAllocBreak + (j * PAGE_SIZE));
                }
                return NULL; // Out of memory
            }
        }
        set_allocation_size_info((uint32)kheapPageAllocBreak, pages_needed); 
        kheapPageAllocBreak += (pages_needed * PAGE_SIZE); 
        return ptr;
    }

    return NULL;
}


void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #1 kmalloc
	//Your code is here
	//Comment the following line
	if(size == 0 ) 
		return 0;
	
	if(size <= DYN_ALLOC_MAX_BLOCK_SIZE)  // block allocator
        return alloc_block(size);
	else {
		return page_allocator_fast(size);
		// return page_allocator(size);
	}
	//TODO: [PROJECT'25.BONUS#3] FAST PAGE ALLOCATOR
}

//=================================
// [2] FREE SPACE FROM KERNEL HEAP:
//=================================
void page_free(void* virtual_address)
{
    uint32 va = (uint32)virtual_address;
    uint32* ptr_page_table = NULL;

    struct FrameInfo* first_frame_info = get_frame_info(ptr_page_directory, va, &ptr_page_table);    
    if (first_frame_info == NULL) 
        panic("kfree(page_free): trying to free an unmapped address!");
    
    uint32 pages_to_free = first_frame_info->num_of_allocated_pages;
    if (pages_to_free == 0) 
        panic("kfree(page_free): trying to free address that is not the start of an allocation, or double free!");
    for (uint32 i = 0; i < pages_to_free; i++) 
        unmap_frame(ptr_page_directory, va + (i * PAGE_SIZE));
    
    first_frame_info->num_of_allocated_pages = 0; 

    // MERGING FREE CHUNKS
    uint32 chunk_start = va;
    uint32 chunk_pages = pages_to_free;
    struct PageChunk *prev_chunk = NULL;
    struct PageChunk *next_chunk = NULL;
    struct PageChunk *it;
    struct PageChunk *final_chunk = NULL; // This will be the new/merged chunk

    // Find adjacent chunks in the free list
    LIST_FOREACH(it, &kheap_page_free_list) {
        // chunk after 
        if (it->start == chunk_start + (chunk_pages * PAGE_SIZE)) 
            next_chunk = it;
        // chunk before 
        if (it->start + (it->num_of_pages * PAGE_SIZE) == chunk_start)
            prev_chunk = it;   
    }
    // 4. Perform merge
    if (prev_chunk != NULL && next_chunk != NULL) { // BOTH
        prev_chunk->num_of_pages += chunk_pages + next_chunk->num_of_pages;
        LIST_REMOVE(&kheap_page_free_list, next_chunk);
        free_block(next_chunk); // Free the PageChunk struct itself
        final_chunk = prev_chunk; 
    }
    else if (prev_chunk != NULL) { // PREV
        prev_chunk->num_of_pages += chunk_pages;
        final_chunk = prev_chunk;
    }
    else if (next_chunk != NULL) { // NEXT
        next_chunk->start = chunk_start;
        next_chunk->num_of_pages += chunk_pages;
        final_chunk = next_chunk;
    }
    else { // No merge,create new chunk and add it to the list
        struct PageChunk* new_chunk = (struct PageChunk*)alloc_block(sizeof(struct PageChunk));
        if (new_chunk == NULL) 
            panic("page_free: out of memory for PageChunk struct!");
        new_chunk->start = chunk_start;
        new_chunk->num_of_pages = chunk_pages;
        
        LIST_INSERT_HEAD(&kheap_page_free_list, new_chunk);
        final_chunk = new_chunk;
    }

    // update kheapPageAllocBreak
    if (final_chunk->start + (final_chunk->num_of_pages * PAGE_SIZE) == kheapPageAllocBreak) {
        kheapPageAllocBreak = final_chunk->start;
        LIST_REMOVE(&kheap_page_free_list, final_chunk);
        free_block(final_chunk);
    }
}
void kfree(void* virtual_address)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #2 kfree
	//Your code is here
	if (virtual_address == NULL)
        return;

    uint32 va = (uint32)virtual_address;
    if (va >= dynAllocStart && va < dynAllocEnd) 
        free_block(virtual_address);
    else if (va >= kheapPageAllocStart && va < KERNEL_HEAP_MAX)
        page_free(virtual_address);  
    else
        panic("kfree() called on an invalid address!");
    
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

static uint32 kheap_get_allocated_size(void* virtual_address)
{
    uint32* ptr_table;
    struct FrameInfo* fi = get_frame_info(ptr_page_directory, (uint32)virtual_address, &ptr_table);
    if (fi == NULL || fi->num_of_allocated_pages == 0) 
        return 0; 
    return (fi->num_of_allocated_pages * PAGE_SIZE);
}

void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - krealloc
	//Your code is here
	//Comment the following line
	// panic("krealloc() is not implemented yet...!!");
    if(virtual_address == NULL)
        return kmalloc(new_size);
    if(new_size == 0) {
        kfree(virtual_address);
        return NULL;
    }
    uint32 old_size;
    bool old_is_block = 
        ((uint32)virtual_address >= KERNEL_HEAP_START && (uint32)virtual_address < kheapPageAllocStart);
    bool new_is_block = (new_size <= DYN_ALLOC_MAX_BLOCK_SIZE);

    if (old_is_block) 
        old_size = get_block_size(virtual_address);
    else {
        old_size = kheap_get_allocated_size(virtual_address);
        if(old_size == 0) panic("krealloc(): invalid page address address!");
    }

  
    // 1. block (stays) 
    if (old_is_block && new_is_block) {
        if(old_size == new_size)
            return virtual_address; 
        return realloc_block(virtual_address, new_size);
    }

    // 2. page (stays)
    if (!old_is_block && !new_is_block) {
        if(old_size == ROUNDUP(new_size, PAGE_SIZE))
            return virtual_address;
        uint32 new_aligned_size = ROUNDUP(new_size, PAGE_SIZE);
        uint32 old_aligned_size = old_size; 
        
        // inplace shrink
        if (new_aligned_size < old_aligned_size) {
            uint32 va_to_free = (uint32)virtual_address + new_aligned_size;
            uint32 diff_size = old_aligned_size - new_aligned_size;
            struct FrameInfo* fiOld = get_frame_info(ptr_page_directory, (uint32)virtual_address, NULL);
            struct FrameInfo* fiNew = get_frame_info(ptr_page_directory, va_to_free, NULL);
            if (fiOld == NULL || fiNew == NULL) 
                panic("krealloc(): invalid page address during inplace shrink");
            
            uint32 pages_kept = new_aligned_size / PAGE_SIZE;
            uint32 pages_to_free = diff_size / PAGE_SIZE;
            fiOld->num_of_allocated_pages = pages_kept;
            fiNew->num_of_allocated_pages = pages_to_free;
            kfree((void*)va_to_free);
            return virtual_address;
        
        }
        // inplace grow 
        if(new_aligned_size > old_aligned_size) {
            uint32 other_va = (uint32)virtual_address + old_aligned_size;
            uint32 diff_pages = (new_aligned_size - old_aligned_size) / PAGE_SIZE;
            struct PageChunk* it;
            struct PageChunk* found = NULL;
            LIST_FOREACH(it, &kheap_page_free_list) {
                if (it->start == other_va) {
                    found = it;
                    break;
                }
            }

            if (found != NULL && found->num_of_pages >= diff_pages) {
                LIST_REMOVE(&kheap_page_free_list, found);   
                for (uint32 i = 0; i < diff_pages; i++) {
                    uint32 va = other_va + (i * PAGE_SIZE);
                    int ret = alloc_page(ptr_page_directory, va, PERM_WRITEABLE, 1);
                    if (ret == E_NO_MEM) panic("krealloc: out of mem during in-place grow!");
                }
                
                // merge
                struct FrameInfo* fiOld = get_frame_info(ptr_page_directory, (uint32)virtual_address, NULL);
                struct FrameInfo* fiNew = get_frame_info(ptr_page_directory, (uint32)other_va, NULL);
                if (fiOld) fiOld->num_of_allocated_pages += diff_pages;
                if (fiNew) fiNew->num_of_allocated_pages = 0; // Merge complete

                if (found->num_of_pages > diff_pages) {
                    found->start += (diff_pages * PAGE_SIZE);
                    found->num_of_pages -= diff_pages;
                    LIST_INSERT_HEAD(&kheap_page_free_list, found);
                } 
                else free_block(found); 

                return virtual_address;
            }

            // extend break
            if (other_va == kheapPageAllocBreak && (kheapPageAllocBreak + (diff_pages * PAGE_SIZE) <= KERNEL_HEAP_MAX)) {
                bool fail = 0;
                for (uint32 i = 0; i < diff_pages; i++) {
                    uint32 va = kheapPageAllocBreak + (i * PAGE_SIZE);
                    int ret = alloc_page(ptr_page_directory, va, PERM_WRITEABLE, 1);
                    if (ret == E_NO_MEM) {
                        for (uint32 j = 0; j < i; j++) 
                            unmap_frame(ptr_page_directory, kheapPageAllocBreak + (j * PAGE_SIZE));
                        fail = 1;
                        break;
                    }
                    struct FrameInfo* fiNew = get_frame_info(ptr_page_directory, va, NULL);
                    if (fiNew) fiNew->num_of_allocated_pages = 0;
                }
                if (!fail) { // extend break
                    kheapPageAllocBreak += (diff_pages * PAGE_SIZE);
                    struct FrameInfo* fiOld = get_frame_info(ptr_page_directory, (uint32)virtual_address, NULL);
                    if (fiOld) fiOld->num_of_allocated_pages = new_aligned_size / PAGE_SIZE;
                    return virtual_address;
                }   
            }
            // move to other place
            void* new_va = kmalloc(new_size);
            if (new_va == NULL) return NULL; 
            memcpy(new_va, virtual_address, old_size); 
            kfree(virtual_address);
            return new_va;
        }   
    }

    // 3 Block to Page (Grow)
    if (old_is_block && !new_is_block) {
        void* new_va = kmalloc(new_size);
        if (new_va == NULL) return NULL;
        memcpy(new_va, virtual_address, old_size); 
        kfree(virtual_address);
        return new_va;
    }

    // 4 Page to Block (shrink)
    if (!old_is_block && new_is_block) {
        void* new_va = kmalloc(new_size);
        if (new_va == NULL) return NULL;
        memcpy(new_va, virtual_address, new_size); 
        kfree(virtual_address);
        return new_va;
    }
    
    // shouldnt happen
    return NULL;
}
