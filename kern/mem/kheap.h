#ifndef FOS_KERN_KHEAP_H_
#define FOS_KERN_KHEAP_H_

#ifndef FOS_KERNEL
# error "This is a FOS kernel header; user programs should not #include it"
#endif

#include <inc/types.h>
#include <inc/queue.h>


/*2017*/
//Values for user heap placement strategy
#define KHP_PLACE_CONTALLOC 0x0
#define KHP_PLACE_FIRSTFIT 	0x1
#define KHP_PLACE_BESTFIT 	0x2
#define KHP_PLACE_NEXTFIT 	0x3
#define KHP_PLACE_WORSTFIT 	0x4
#define KHP_PLACE_CUSTOMFIT 0x5

//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #0 Page Alloc Limits [GIVEN]
extern uint32 kheapPageAllocStart ;
extern uint32 kheapPageAllocBreak ;
extern uint32 kheapPlacementStrategy;

/*2025*/ //Replaced by setter & getter function
static inline void set_kheap_strategy(uint32 strategy){kheapPlacementStrategy = strategy;}
static inline uint32 get_kheap_strategy(){return kheapPlacementStrategy ;}
//***********************************

struct PageChunkNode
{
    uint32 start;
    uint32 num_of_pages; 
    struct PageChunkNode *parent_size, *left_size, *right_size;
    struct PageChunkNode *parent_addr, *left_addr, *right_addr;
};

struct PageChunk
{
    uint32 start;      // The starting virtual address of the hole
    uint32 num_of_pages; // number of pages in the hole
    LIST_ENTRY(PageChunk) prev_next_info; // Link for the free list
};

// The head of the free list
LIST_HEAD(PageChunk_List, PageChunk);
extern struct PageChunk_List kheap_page_free_list;

// fast lists ( trees )
// Root of the tree ordered by num_of_pages
extern struct PageChunkNode* kheap_free_tree_by_size;
// Root of the tree ordered by start address
extern struct PageChunkNode* kheap_free_tree_by_addr;
//***********************************
void kheap_init();

void* kmalloc(unsigned int size);
void kfree(void* virtual_address);
void *krealloc(void *virtual_address, unsigned int new_size);

unsigned int kheap_virtual_address(unsigned int physical_address);
unsigned int kheap_physical_address(unsigned int virtual_address);

int numOfKheapVACalls ;


#endif // FOS_KERN_KHEAP_H_
