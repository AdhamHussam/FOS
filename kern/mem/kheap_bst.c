#include "kheap_bst.h"
#include <inc/types.h>
#include <inc/assert.h>

extern struct PageChunkNode* kheap_free_tree_by_size;
extern struct PageChunkNode* kheap_free_tree_by_addr;

//==============//
//  Size Tree   //
//==============//

void bst_insert_by_size(struct PageChunkNode* node) {
    node->left_size = node->right_size = node->parent_size = NULL;
    if (kheap_free_tree_by_size == NULL) {
        kheap_free_tree_by_size = node;
        return;
    }
    struct PageChunkNode* current = kheap_free_tree_by_size;
    while (1) {
        if (node->num_of_pages >= current->num_of_pages) {
            if (current->right_size == NULL) {
                current->right_size = node;
                node->parent_size = current;
                return;
            }
            current = current->right_size;
        }
        else {
            if (current->left_size == NULL) {
                current->left_size = node;
                node->parent_size = current;
                return;
            }
            current = current->left_size;
        }
    }
}

static void bst_replace_node_in_parent_size(struct PageChunkNode* node, struct PageChunkNode* new_node) {
    if (node->parent_size == NULL) {
        kheap_free_tree_by_size = new_node;
    } 
    else if (node == node->parent_size->left_size) {
        node->parent_size->left_size = new_node;
    }
    else {
        node->parent_size->right_size = new_node;
    }
    if (new_node != NULL) {
        new_node->parent_size = node->parent_size;
    }
}

static struct PageChunkNode* bst_find_min_size(struct PageChunkNode* node) {
    while (node->left_size != NULL) {
        node = node->left_size;
    }
    return node;
}

void bst_remove_by_size(struct PageChunkNode* node) {
    if (node->left_size == NULL) {
        bst_replace_node_in_parent_size(node, node->right_size);
    } 
    else if (node->right_size == NULL) {
        bst_replace_node_in_parent_size(node, node->left_size);
    }
    else {
        struct PageChunkNode* successor = bst_find_min_size(node->right_size);
        if (successor->parent_size != node) {
            bst_replace_node_in_parent_size(successor, successor->right_size);
            successor->right_size = node->right_size;
            successor->right_size->parent_size = successor;
        }
        bst_replace_node_in_parent_size(node, successor);
        successor->left_size = node->left_size;
        successor->left_size->parent_size = successor;
    }
}

struct PageChunkNode* bst_find_exact_fit(uint32 pages_needed) {
    struct PageChunkNode* current = kheap_free_tree_by_size;
    while (current != NULL) {
        if (pages_needed == current->num_of_pages) {
            return current;
        } 
        else if (pages_needed < current->num_of_pages) {
            current = current->left_size;
        }
         else {
            current = current->right_size;
        }
    }
    return NULL;
}

struct PageChunkNode* bst_find_worst_fit(uint32 pages_needed) {
    struct PageChunkNode* current = kheap_free_tree_by_size;
    struct PageChunkNode* worst_fit = NULL;    
    while (current != NULL) {
        if (current->num_of_pages >= pages_needed) {
            worst_fit = current;
            current = current->right_size;
        } 
        else { // too small go left
            current = current->left_size;
        }
    }
    return worst_fit;
}

//==============//
// Address Tree //
//==============//

void bst_insert_by_addr(struct PageChunkNode* node) {
    node->left_addr = node->right_addr = node->parent_addr = NULL;
    if (kheap_free_tree_by_addr == NULL) {
        kheap_free_tree_by_addr = node;
        return;
    }
    struct PageChunkNode* current = kheap_free_tree_by_addr;
    while (1) {
        if (node->start > current->start) {
            if (current->right_addr == NULL) {
                current->right_addr = node;
                node->parent_addr = current;
                return;
            }
            current = current->right_addr;
        } 
        else {
            if (current->left_addr == NULL) {
                current->left_addr = node;
                node->parent_addr = current;
                return;
            }
            current = current->left_addr;
        }
    }
}

static void bst_replace_node_in_parent_addr(struct PageChunkNode* node, struct PageChunkNode* new_node) {
    if (node->parent_addr == NULL) {
        kheap_free_tree_by_addr = new_node;
    } 
    else if (node == node->parent_addr->left_addr) {
        node->parent_addr->left_addr = new_node;
    } 
    else {
        node->parent_addr->right_addr = new_node;
    }
    if (new_node != NULL) {
        new_node->parent_addr = node->parent_addr;
    }
}

static struct PageChunkNode* bst_find_min_addr(struct PageChunkNode* node) {
    while (node->left_addr != NULL) {
        node = node->left_addr;
    }
    return node;
}

void bst_remove_by_addr(struct PageChunkNode* node) {
    if (node->left_addr == NULL) {
        bst_replace_node_in_parent_addr(node, node->right_addr);
    } 
    else if (node->right_addr == NULL) {
        bst_replace_node_in_parent_addr(node, node->left_addr);
    }
    else {
        struct PageChunkNode* successor = bst_find_min_addr(node->right_addr);
        if (successor->parent_addr != node) {
            bst_replace_node_in_parent_addr(successor, successor->right_addr);
            successor->right_addr = node->right_addr;
            successor->right_addr->parent_addr = successor;
        }
        bst_replace_node_in_parent_addr(node, successor);
        successor->left_addr = node->left_addr;
        successor->left_addr->parent_addr = successor;
    }
}

struct PageChunkNode* bst_find_prev_neighbor(uint32 addr) {
    struct PageChunkNode* current = kheap_free_tree_by_addr;
    struct PageChunkNode* neighbor = NULL;
    while (current != NULL) {
        if (current->start < addr) {
            neighbor = current;
            current = current->right_addr;
        } 
        else {
            current = current->left_addr;
        }
    }
    return neighbor;
}

struct PageChunkNode* bst_find_next_neighbor(uint32 addr) {
    struct PageChunkNode* current = kheap_free_tree_by_addr;
    struct PageChunkNode* neighbor = NULL;
    while (current != NULL) {
        if (current->start > addr) {
            neighbor = current;
            current = current->left_addr;
        } 
        else {
            current = current->right_addr;
        }
    }
    return neighbor;
}