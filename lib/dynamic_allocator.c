/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//==================================
//==================================
// [1] GET PAGE VA:
//==================================
__inline__ uint32 to_page_va(struct PageInfoElement *ptrPageInfo)
{
	if (ptrPageInfo < &pageBlockInfoArr[0] || ptrPageInfo >= &pageBlockInfoArr[DYN_ALLOC_MAX_SIZE/PAGE_SIZE])
			panic("to_page_va called with invalid pageInfoPtr");
	//Get start VA of the page from the corresponding Page Info pointer
	int idxInPageInfoArr = (ptrPageInfo - pageBlockInfoArr);
	return dynAllocStart + (idxInPageInfoArr << PGSHIFT);
}

//==================================
// [2] GET PAGE INFO OF PAGE VA:
//==================================
__inline__ struct PageInfoElement * to_page_info(uint32 va)
{
	int idxInPageInfoArr = (va - dynAllocStart) >> PGSHIFT;
	if (idxInPageInfoArr < 0 || idxInPageInfoArr >= DYN_ALLOC_MAX_SIZE/PAGE_SIZE)
		panic("to_page_info called with invalid pa");
	return &pageBlockInfoArr[idxInPageInfoArr];
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
bool is_initialized = 0;
void initialize_dynamic_allocator(uint32 daStart, uint32 daEnd)
{
    //==================================================================================
    //DON'T CHANGE THESE LINES==========================================================
    //==================================================================================
    {
        assert(daEnd <= daStart + DYN_ALLOC_MAX_SIZE);
        is_initialized = 1;
    }
    //==================================================================================
    //==================================================================================
    //TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #1 initialize_dynamic_allocator
    //Your code is here

    //initializing start and end
    dynAllocStart = daStart;
    dynAllocEnd = daEnd;

    //initializing free page list
    LIST_INIT(&freePagesList);

    //initializing page info array
    int ps = PAGE_SIZE;
    int page_info_array_size = (daEnd-daStart)/ps;
    for(int i=0;i<page_info_array_size;i++){
        pageBlockInfoArr[i].block_size = 0;
        pageBlockInfoArr[i].num_of_free_blocks = 0;
        LIST_INSERT_TAIL(&freePagesList, &pageBlockInfoArr[i]);
    }


    //initializing free block list
    int free_blk_list_array_size = LOG2_MAX_SIZE - LOG2_MIN_SIZE + 1;
    for(int i=0;i<free_blk_list_array_size;i++){
        LIST_INIT(&freeBlockLists[i]);
    }

    //Comment the following line
    //panic("initialize_dynamic_allocator() Not implemented yet");

}

//===========================
// [2] GET BLOCK SIZE:
//===========================
__inline__ uint32 get_block_size(void *va)
{
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #2 get_block_size
	//Your code is here
	int nowPage = ((unsigned int) va - dynAllocStart) / PAGE_SIZE;
	int nowData = pageBlockInfoArr[nowPage].block_size;
	return nowData;
	//Comment the following line
	//panic("get_block_size() Not implemented yet");
}

//===========================
// 3) ALLOCATE BLOCK:
//===========================
void *alloc_block(uint32 size)
{
    //==================================================================================
    //DON'T CHANGE THESE LINES==========================================================
    //==================================================================================
    {
        assert(size <= DYN_ALLOC_MAX_BLOCK_SIZE);
    }
    //==================================================================================
    //==================================================================================
    //TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #3 alloc_block
    //Your code is here
	uint32 req = size;
	uint32 nearestPof2 = 8;
	int idx = 0;
	if (req == 0)
		return NULL;

	//find nearest power of 2
	while (nearestPof2 < req) {
		nearestPof2 *= 2;
		idx++;
	}

	if (LIST_SIZE(&freeBlockLists[idx]) > 0) {
		//case one: a free block exists
		struct BlockElement *b = LIST_FIRST(&freeBlockLists[idx]);
		struct PageInfoElement *p = to_page_info((uint32) b);
		p->num_of_free_blocks -= 1;
		LIST_REMOVE(&freeBlockLists[idx], b);
		return (void*) b;
	}
	if (LIST_SIZE(&freePagesList) > 0) {
		//case two: a free page exists
		struct PageInfoElement *p = LIST_FIRST(&freePagesList);
		uint32 va = to_page_va(p);
		get_page((void*) va);
		p->block_size = nearestPof2;
		p->num_of_free_blocks = (PAGE_SIZE / nearestPof2);
		
		//split page into blocks
		for (uint32 i = va; i < va + PAGE_SIZE; i += nearestPof2) {
			struct BlockElement *blockva = (struct BlockElement*) i;
			LIST_INSERT_TAIL(&freeBlockLists[idx], blockva);
		}
		
		LIST_REMOVE(&freePagesList, p);
		struct BlockElement *b = LIST_FIRST(&freeBlockLists[idx]);
		p->num_of_free_blocks -= 1;
		LIST_REMOVE(&freeBlockLists[idx], b);
		return (void*) b;
	}
	//case three: allocate block from next list
	int sz = LOG2_MAX_SIZE - LOG2_MIN_SIZE + 1;
	for (int i = idx; i < sz; i++) {
		if (LIST_SIZE(&freeBlockLists[i]) > 0) {
			struct BlockElement *b = LIST_FIRST(&freeBlockLists[i]);
			struct PageInfoElement *p = to_page_info((uint32) b);
			p->num_of_free_blocks -= 1;
			LIST_REMOVE(&freeBlockLists[i], b);
			return (void*) b;
		}
	}

	//found nothing
	return NULL;

    //Comment the following line
    //panic("alloc_block() Not implemented yet");
    //TODO: [PROJECT'25.BONUS#1] DYNAMIC ALLOCATOR - block if no free block
}
//===========================
// [4] FREE BLOCK:
//===========================
void free_block(void *va)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert((uint32)va >= dynAllocStart && (uint32)va < dynAllocEnd);
	}
	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #4 free_block
	//Your code is here
	if(va==NULL)
		return;
	unsigned int blkSz = get_block_size(va);
	unsigned int nearestPof2 = 8;
	int idx = 0;
	if (blkSz == 0)
		return;
	while (nearestPof2 < blkSz)
	{
		nearestPof2 *= 2;
		idx++;
	}

	int nowPage = ((unsigned int) va - dynAllocStart) / PAGE_SIZE;
	struct BlockElement *b = (struct BlockElement*)va;
	LIST_INSERT_HEAD(&freeBlockLists[idx] , b);
    pageBlockInfoArr[nowPage].num_of_free_blocks += 1;

    unsigned int totBlks = PAGE_SIZE / blkSz;
    if(pageBlockInfoArr[nowPage].num_of_free_blocks == totBlks)
    {
    	struct BlockElement *cur = LIST_FIRST(&freeBlockLists[idx]);
    	while(cur != NULL)
    	{
        	struct BlockElement *nxt = LIST_NEXT(cur);
        	if (((unsigned int)cur - dynAllocStart) / PAGE_SIZE == nowPage)
			{
				LIST_REMOVE(&freeBlockLists[idx], cur);
			}
        	cur = nxt;
    	}
    LIST_INSERT_TAIL(&freePagesList, &pageBlockInfoArr[nowPage]);
    pageBlockInfoArr[nowPage].block_size = 0;
    pageBlockInfoArr[nowPage].num_of_free_blocks = 0;

    return_page((void*)to_page_va(&pageBlockInfoArr[nowPage]));
    }
	//Comment the following line
	//panic("free_block() Not implemented yet");
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] REALLOCATE BLOCK:
//===========================
void *realloc_block(void* va, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - realloc_block
	//Your code is here
	if(va==NULL)
		return alloc_block(new_size);
	if(new_size==0){
		free_block(va);
		return NULL;
	}

	void* newVAdress = alloc_block(new_size);

	if(newVAdress!=NULL){
		void* source = va;
		void* dest = newVAdress;
		uint32 sSize = (uint32)get_block_size(va);
		uint32 nSize = (uint32)new_size;
		int dSize;
		if(sSize < nSize) dSize = sSize;
		else dSize = nSize;
		memmove(source,dest,dSize);
		free_block(va);
	}

	return newVAdress;
	//Comment the following line
	//panic("realloc_block() Not implemented yet");
}