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
//============================ HELPER FUNCTIONS ====================================//
//==================================================================================//

// Helper to set the FrameInfo data so kfree knows the size and kheap_virtual_address works
static void set_allocation_size_info(uint32 start_va, uint32 pages_needed)
{
    uint32* ptr_page_table;
    struct FrameInfo* first_frame_info = get_frame_info(ptr_page_directory, start_va, &ptr_page_table);
    
    if (first_frame_info != NULL) {
        first_frame_info->num_of_allocated_pages = pages_needed;
        
        // We also set the mapped address for all frames to support kheap_virtual_address lookup
        for (uint32 i = 0; i < pages_needed; i++) {
            struct FrameInfo* fi = get_frame_info(ptr_page_directory, start_va + (i * PAGE_SIZE), &ptr_page_table);
            if(fi) fi->mapped_address = start_va + (i * PAGE_SIZE);
        }
    }
    else panic("set_allocation_size_info: first frame is not mapped!");
}

// Helper to remove a node from the linked list by address (Syncs List with Tree)
static void find_and_remove_from_list(uint32 start_addr)
{
    struct PageChunk *item;
    LIST_FOREACH(item, &kheap_page_free_list) {
        if (item->start == start_addr) {
            LIST_REMOVE(&kheap_page_free_list, item);
            free_block(item); // Free the metadata block of the list item
            return;
        }
    }
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===================================
// [1] ALLOCATE SPACE IN KERNEL HEAP:
//===================================
// FAST KMALLOC USING BST INSERTIONS AND EXACT AND MAX SELECTIONS
static void* page_allocator_fast(unsigned int size)
{
    uint32 pages_needed = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;

	// 1. Exact fit
    struct PageChunkNode* found = bst_find_exact_fit(pages_needed);
	
	// 2. Worst fit
    if (found == NULL) 
	    found = bst_find_worst_fit(pages_needed);
    
    if (found != NULL) {
        void* allocate_from = (void*)found->start;

        bst_remove_by_size(found);
        bst_remove_by_addr(found);        
        find_and_remove_from_list(found->start);

        if (found->num_of_pages > pages_needed) {

            found->start += (pages_needed * PAGE_SIZE);
            found->num_of_pages -= pages_needed;
    
            bst_insert_by_size(found);
            bst_insert_by_addr(found);
            
            struct PageChunk* new_list_node = (struct PageChunk*)alloc_block(sizeof(struct PageChunk));
            new_list_node->start = found->start;
            new_list_node->num_of_pages = found->num_of_pages;
            LIST_INSERT_HEAD(&kheap_page_free_list, new_list_node);
        }
        else {
            free_block(found);
        }

        for (uint32 i = 0; i < pages_needed; i++) {
            uint32 va = (uint32)allocate_from + (i * PAGE_SIZE);
            int ret = alloc_page(ptr_page_directory, va, PERM_WRITEABLE, 1);
            if (ret == E_NO_MEM) panic("kmalloc_fast: Out of physical memory!");
        }
        
        set_allocation_size_info((uint32)allocate_from, pages_needed);
        return allocate_from;
    }
    
    // 3. Extend the Break
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
        set_allocation_size_info((uint32)kheapPageAllocBreak, pages_needed);
        kheapPageAllocBreak += (pages_needed * PAGE_SIZE);
        return ptr;
    }
    
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
        
        // set_allocation_size_info((uint32)allocate_from, pages_needed); 
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
        // set_allocation_size_info((uint32)kheapPageAllocBreak, pages_needed); 
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
		// return page_allocator(size);
		return page_allocator_fast(size);
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
        return; 
    
    uint32 pages_to_free = first_frame_info->num_of_allocated_pages;
    if (pages_to_free == 0)
         return; 
    
    for (uint32 i = 0; i < pages_to_free; i++) 
        unmap_frame(ptr_page_directory, va + (i * PAGE_SIZE));
    
    first_frame_info->num_of_allocated_pages = 0; 

    uint32 chunk_start = va;
    uint32 chunk_pages = pages_to_free;
    
    struct PageChunkNode* prev_node = bst_find_prev_neighbor(chunk_start);
    struct PageChunkNode* next_node = bst_find_next_neighbor(chunk_start);
    
    bool merge_prev = (prev_node && (prev_node->start + prev_node->num_of_pages * PAGE_SIZE == chunk_start));
    bool merge_next = (next_node && (chunk_start + chunk_pages * PAGE_SIZE == next_node->start));

    struct PageChunkNode* final_node = NULL;

    if (merge_prev && merge_next) { // Merge BOTH
        bst_remove_by_size(prev_node); bst_remove_by_addr(prev_node);
        find_and_remove_from_list(prev_node->start);

        bst_remove_by_size(next_node); bst_remove_by_addr(next_node);
        find_and_remove_from_list(next_node->start);

        prev_node->num_of_pages += chunk_pages + next_node->num_of_pages;
        free_block(next_node); // Free consumed node
        final_node = prev_node; 
    }
    else if (merge_prev) { // Merge PREV
        bst_remove_by_size(prev_node); bst_remove_by_addr(prev_node);
        find_and_remove_from_list(prev_node->start);

        prev_node->num_of_pages += chunk_pages;
        final_node = prev_node;
    }
    else if (merge_next) { // Merge NEXT
        bst_remove_by_size(next_node); bst_remove_by_addr(next_node);
        find_and_remove_from_list(next_node->start);

        next_node->start = chunk_start;
        next_node->num_of_pages += chunk_pages;
        final_node = next_node;
    }
    else { // No merge, create new node
        struct PageChunkNode* new_chunk = (struct PageChunkNode*)alloc_block(sizeof(struct PageChunkNode));
        if (new_chunk == NULL) panic("page_free: out of memory for PageChunk struct!");
        new_chunk->start = chunk_start;
        new_chunk->num_of_pages = chunk_pages;
        final_node = new_chunk;
    }

    // update kheapPageAllocBreak if at end
    if (final_node->start + (final_node->num_of_pages * PAGE_SIZE) == kheapPageAllocBreak) {
        kheapPageAllocBreak = final_node->start;
        // If we merged or created, we must free the metadata node to return memory to OS
        free_block(final_node); 
    }
    else {
        // Add to BSTs
        bst_insert_by_size(final_node);
        bst_insert_by_addr(final_node);
        
        // Add to List (Create separate struct for List)
        struct PageChunk* new_list_node = (struct PageChunk*)alloc_block(sizeof(struct PageChunk));
        new_list_node->start = final_node->start;
        new_list_node->num_of_pages = final_node->num_of_pages;
        LIST_INSERT_HEAD(&kheap_page_free_list, new_list_node);
    }
}
void kfree(void* virtual_address)
{
	//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #2 kfree
	//Your code is here
	//Comment the following line
	// panic("kfree() is not implemented yet...!!");
    if (virtual_address == NULL) return;

    uint32 va = (uint32)virtual_address;
    // Block Allocator Range
    if (va >= KERNEL_HEAP_START && va < dynAllocEnd) 
        free_block(virtual_address);
    // Page Allocator Range
    else if (va >= kheapPageAllocStart && va < KERNEL_HEAP_MAX)
        page_free(virtual_address);  
    else
        panic("kfree() called on an invalid address %x", va);
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
    struct FrameInfo *ptr_frame_info = to_frame_info(physical_address);
	if(ptr_frame_info->references == 0 || ptr_frame_info->mapped_address == 0) 
		return 0;
	// "mapped_address" must be defined in FrameInfo -> inc/memlayout.h
	// "PGOFF" gets the lower 12 bits (Offset)
	uint32 virt_base = ptr_frame_info->mapped_address; 
	uint32 offset = PGOFF(physical_address);
    return virt_base | offset;

	/*EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED */
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
	uint32* ptr_page_table;
    get_page_table(ptr_page_directory, virtual_address, &ptr_page_table);
    
    if (ptr_page_table != NULL) {
        uint32 entry = ptr_page_table[PTX(virtual_address)];
        if (entry & PERM_PRESENT) {
            // EXTRACT_ADDRESS gets the Frame Address (top 20 bits)
            // PGOFF gets the Offset (bottom 12 bits)
            return EXTRACT_ADDRESS(entry) | PGOFF(virtual_address);
        }
    }
    return 0;
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
    
    uint32 va = (uint32)virtual_address;
    
    // 1. Block Allocator Logic
    if (va >= KERNEL_HEAP_START && va < dynAllocEnd) {
        if (new_size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
             return realloc_block(virtual_address, new_size);
        } 
        else {
             // Grow from Block to Page
             void* new_ptr = kmalloc(new_size);
             if (!new_ptr) return NULL;
             
             uint32 old_size = get_block_size(virtual_address); 
             memcpy(new_ptr, virtual_address, old_size);
             free_block(virtual_address);
             return new_ptr;
        }
    }
    // 2. Page Allocator Logic
    else if (va >= kheapPageAllocStart && va < KERNEL_HEAP_MAX) {
        if (new_size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
            // Shrink from Page to Block
            void* new_ptr = kmalloc(new_size);
            if (!new_ptr) return NULL;
            
            memcpy(new_ptr, virtual_address, new_size); 
            kfree(virtual_address);
            return new_ptr;
        }
        else {
            // Page to Page resizing
            uint32 old_size = kheap_get_allocated_size(virtual_address);
            uint32 new_pages = ROUNDUP(new_size, PAGE_SIZE) / PAGE_SIZE;
            uint32 old_pages = old_size / PAGE_SIZE;

            if (new_pages == old_pages) return virtual_address;

            if (new_pages < old_pages) {
                // Shrink in place
                uint32 diff = old_pages - new_pages;
                for (uint32 i = 0; i < diff; i++) {
                    unmap_frame(ptr_page_directory, va + ((old_pages - 1 - i) * PAGE_SIZE));
                }

                uint32* ptr_table;
                struct FrameInfo* fi = get_frame_info(ptr_page_directory, va, &ptr_table);
                fi->num_of_allocated_pages = new_pages;

                return virtual_address; 
            } 
            else {
                // Grow in place
                void* new_ptr = kmalloc(new_size);
                if (!new_ptr) return NULL;
                memcpy(new_ptr, virtual_address, old_size);
                kfree(virtual_address);
                return new_ptr;
            }
        }
    }
    
    return NULL;
}
