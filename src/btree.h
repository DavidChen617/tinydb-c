#ifndef BTREE_H
#define BTREE_H

#include <stdint.h>
#include <stdbool.h>
#include "cursor.h"

// Node type
typedef enum
{
    NODE_INTERNAL,
    NODE_LEAF
} NodeType;

// Common header
#define NODE_TYPE_SIZE 1
#define IS_ROOT_SIZE 1
#define PARENT_POINTER_SIZE 4
#define COMMON_NODE_HEADER_SIZE (NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE)

// leaf node header
#define LEAF_NODE_NUM_CELLS_SIZE    4
#define LEAF_NODE_NEXT_LEAF_SIZE    4
#define LEAF_NODE_NUM_CELLS_OFFSET  COMMON_NODE_HEADER_SIZE
#define LEAF_NODE_NEXT_LEAF_OFFSET  (COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE)
#define LEAF_NODE_HEADER_SIZE       (COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE + LEAF_NODE_NEXT_LEAF_SIZE)

// leaf node cell
#define LEAF_NODE_KEY_SIZE 4
#define LEAF_NODE_VALUE_SIZE ROW_SIZE
#define LEAF_NODE_CELL_SIZE (LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE)
#define LEAF_NODE_SPACE_FOR_CELLS (PAGE_SIZE - LEAF_NODE_HEADER_SIZE)
#define LEAF_NODE_MAX_CELLS (LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE)

// split contant
#define LEAF_NODE_RIGHT_SPLIT_COUNT ((LEAF_NODE_MAX_CELLS + 1) / 2)
#define LEAF_NODE_LEFT_SPLIT_COUNT ((LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT)

// interanl node header
#define INTERNAL_NODE_NUM_KEYS_SIZE 4
#define INTERNAL_NODE_RIGHT_CHILD_SIZE 4
#define INTERNAL_NODE_HEADER_SIZE (COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE)

// internal node cell
#define INTERNAL_NODE_KEY_SIZE 4
#define INTERNAL_NODE_CHILD_SIZE 4
#define INTERNAL_NODE_CELL_SIZE (INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE)
#define INTERNAL_NODE_MAX_KEYS ((PAGE_SIZE - INTERNAL_NODE_HEADER_SIZE) / INTERNAL_NODE_CELL_SIZE)
#define INTERNAL_NODE_MAX_CELLS 3
#define INVALID_PAGE_NUM UINT32_MAX

// leaf node access function
uint32_t *leaf_node_num_cells(void *node);
void *leaf_node_cell(void *node, uint32_t cell_num);
uint32_t *leaf_node_key(void *node, uint32_t cell_num);
void *leaf_node_value(void *node, uint32_t cell_num);
void initialize_leaf_node(void *node);
void leaf_node_insert(Cursor *cursor, uint32_t key, Row *value);
Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key);

// node type / root access
NodeType get_node_type(void *node);
void set_node_type(void *node, NodeType type);
bool is_node_root(void *node);
void set_node_root(void *node, bool is_root);

// internal node access
uint32_t *internal_node_num_keys(void *node);
uint32_t *internal_node_right_child(void *node);
uint32_t *internal_node_child(void *node, uint32_t child_num);
uint32_t *internal_node_key(void *node, uint32_t key_num);
uint32_t *internal_node_cell(void* node, uint32_t cell_num);
void initialize_internal_node(void *node);

// spilt
uint32_t get_unused_page_num(Pager *pager);
void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, Row *value);
void create_new_root(Table *table, uint32_t right_child_page_num);
void internal_node_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num);

uint32_t *leaf_node_next_leaf(void *node);
Cursor *internal_node_find(Table *table, uint32_t page_num, uint32_t key);
Cursor *table_find(Table *table, uint32_t key);

void indent(uint32_t level);
void print_tree(Pager *pager, uint32_t page_num, uint32_t indent);

void internal_node_split_and_insert(Table* table, uint32_t parent_page_num, uint32_t child_page_num);

#endif
