#ifndef FOS_KERN_KHEAP_BST_H_
#define FOS_KERN_KHEAP_BST_H_

#include "kheap.h"

// BST insertion/removal helpers
void bst_insert_by_size(struct PageChunkNode* node);
void bst_insert_by_addr(struct PageChunkNode* node);
void bst_remove_by_size(struct PageChunkNode* node);
void bst_remove_by_addr(struct PageChunkNode* node);

// Allocator-specific search functions
struct PageChunkNode* bst_find_exact_fit(uint32 pages_needed);
struct PageChunkNode* bst_find_worst_fit(uint32 pages_needed);
struct PageChunkNode* bst_find_prev_neighbor(uint32 addr);
struct PageChunkNode* bst_find_next_neighbor(uint32 addr);

#endif /* FOS_KERN_KHEAP_BST_H_ */