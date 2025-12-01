#include <inc/lib.h>
#include <inc/queue.h>

struct UserHeapChunk
{
    uint32 start_addr;
    uint32 size;
    LIST_ENTRY(UserHeapChunk) prev_next_info;
};

LIST_HEAD(UserHeapChunk_List, UserHeapChunk);

struct UserHeapChunk_List FreeMemList;
struct UserHeapChunk_List AllocMemList;
//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE USER HEAP:
//==============================================
int __firstTimeFlag = 1;
void uheap_init()
{
	if(__firstTimeFlag)
	{
		initialize_dynamic_allocator(USER_HEAP_START, USER_HEAP_START + DYN_ALLOC_MAX_SIZE);
		uheapPlaceStrategy = sys_get_uheap_strategy();
		uheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		uheapPageAllocBreak = uheapPageAllocStart;

		__firstTimeFlag = 0;
	}
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = __sys_allocate_page(ROUNDDOWN(va, PAGE_SIZE), PERM_USER|PERM_WRITEABLE|PERM_UHPAGE);
	if (ret < 0)
		panic("get_page() in user: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	int ret = __sys_unmap_frame(ROUNDDOWN((uint32)va, PAGE_SIZE));
	if (ret < 0)
		panic("return_page() in user: failed to return a page to the kernel");
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=================================
// [1] ALLOCATE SPACE IN USER HEAP:
//=================================
void* malloc(uint32 size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'25.IM#2] USER HEAP - #1 malloc
	//Your code is here
	//Comment the following line
	// panic("malloc() is not implemented yet...!!");
	 if (size <= DYN_ALLOC_MAX_BLOCK_SIZE)
	        return alloc_block(size);

	uint32 size_in_pages = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
	uint64 required_size = size_in_pages * PAGE_SIZE;// may need uint64

	struct UserHeapChunk *iterator = NULL;
	struct UserHeapChunk *target_chunk = NULL;

	LIST_FOREACH(iterator, &FreeMemList)
	{
		if (iterator->size == required_size)
		{
			target_chunk = iterator;
			break;
		}
	}

	if (target_chunk == NULL)
	{
		LIST_FOREACH(iterator, &FreeMemList)
		{
			if (iterator->size > required_size)
			{
				if (target_chunk == NULL || iterator->size > target_chunk->size)
					target_chunk = iterator;
			}
		}
	}

	if (target_chunk != NULL)
	{
		uint32 allocated_address = target_chunk->start_addr;

		if (target_chunk->size > required_size)
		{
			struct UserHeapChunk *new_hole = (struct UserHeapChunk*)alloc_block(sizeof(struct UserHeapChunk));
			if (new_hole != NULL)
			{
				new_hole->start_addr = target_chunk->start_addr + required_size;
				new_hole->size = target_chunk->size - required_size;
				LIST_INSERT_AFTER(&FreeMemList, target_chunk, new_hole);
			}
			target_chunk->size = required_size;
		}

		LIST_REMOVE(&FreeMemList, target_chunk);
		LIST_INSERT_TAIL(&AllocMemList, target_chunk);

		sys_allocate_user_mem(allocated_address, required_size);
		return (void*)allocated_address;
	}

	if ((uheapPageAllocBreak + required_size ) <= USER_HEAP_MAX )
	{
		uint32 allocated_address = uheapPageAllocBreak;
		uheapPageAllocBreak += required_size;

		sys_allocate_user_mem(allocated_address, required_size);

		struct UserHeapChunk *new_alloc_node = (struct UserHeapChunk*)alloc_block(sizeof(struct UserHeapChunk));
		if (new_alloc_node != NULL)
		{
			new_alloc_node->start_addr = allocated_address;
			new_alloc_node->size = required_size;
			LIST_INSERT_TAIL(&AllocMemList, new_alloc_node);
		}

		return (void*)allocated_address;
	}

	return NULL;
}

//=================================
// [2] FREE SPACE FROM USER HEAP:
//=================================
void free(void* virtual_address)
{
	//TODO: [PROJECT'25.IM#2] USER HEAP - #3 free
	//Your code is here
	//Comment the following line
	//panic("free() is not implemented yet...!!");
	if ((uint32)virtual_address >= USER_HEAP_START && (uint32)virtual_address < dynAllocEnd)
	    {
	        free_block(virtual_address);
	        return;
	    }

	    if ((uint32)virtual_address >= uheapPageAllocStart && (uint32)virtual_address < USER_HEAP_MAX)
	    {
	        struct UserHeapChunk *target_node = NULL;
	        struct UserHeapChunk *iterator = NULL;

	        LIST_FOREACH(iterator, &AllocMemList)
	        {
	            if (iterator->start_addr == (uint32)virtual_address)
	            {
	                target_node = iterator;
	                break;
	            }
	        }

	        if (target_node == NULL) return;

	        LIST_REMOVE(&AllocMemList, target_node);
	        sys_free_user_mem(target_node->start_addr, target_node->size);

	        // COALESCING LOGIC
	        struct UserHeapChunk *new_free_chunk = target_node;
	        struct UserHeapChunk *temp = NULL;

	        // Find insert position (Scan list)
	        // We want to insert 'new_free_chunk' into FreeMemList.
	        // Ideally, we keep the list sorted by address to make merging O(1) after insertion.
	        // But for O(N) scan, we just insert at head and check all.

	        LIST_INSERT_HEAD(&FreeMemList, new_free_chunk);

	        // Merge Pass
	        // We restart the loop after any merge to be safe
	        int merged = 1;
	        while(merged)
	        {
	            merged = 0;
	            LIST_FOREACH(temp, &FreeMemList)
	            {
	                if (temp == new_free_chunk) continue;

	                // Check Next Neighbor
	                if (new_free_chunk->start_addr + new_free_chunk->size == temp->start_addr)
	                {
	                    new_free_chunk->size += temp->size;
	                    LIST_REMOVE(&FreeMemList, temp);
	                    free_block(temp);
	                    merged = 1;
	                    break;
	                }

	                // Check Prev Neighbor
	                if (temp->start_addr + temp->size == new_free_chunk->start_addr)
	                {
	                    temp->size += new_free_chunk->size;
	                    LIST_REMOVE(&FreeMemList, new_free_chunk);
	                    free_block(new_free_chunk);
	                    new_free_chunk = temp;
	                    merged = 1;
	                    break;
	                }
	            }
	        }

	        // Shrink Heap
	        if (new_free_chunk->start_addr + new_free_chunk->size == uheapPageAllocBreak)
	        {
	            uheapPageAllocBreak = new_free_chunk->start_addr;
	            LIST_REMOVE(&FreeMemList, new_free_chunk);
	            free_block(new_free_chunk);
	        }
	    }
	    else
	    {
	        panic("free: Invalid address");
	    }
}

//=================================
// [3] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================

	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #2 smalloc
	//Your code is here
	//Comment the following line
	panic("smalloc() is not implemented yet...!!");
}

//========================================
// [4] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================

	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #4 sget
	//Your code is here
	//Comment the following line
	panic("sget() is not implemented yet...!!");
}


//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================
	panic("realloc() is not implemented yet...!!");
}


//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_delete_shared_object(...); which switches to the kernel mode,
//	calls delete_shared_object(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the delete_shared_object() function is empty, make sure to implement it.
void sfree(void* virtual_address)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - sfree
	//Your code is here
	//Comment the following line
	panic("sfree() is not implemented yet...!!");

	//	1) you should find the ID of the shared variable at the given address
	//	2) you need to call sys_freeSharedObject()
}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//
