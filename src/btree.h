#ifndef BTREE_H
#define BTREE_H

#include <stdint.h>
#include <stdbool.h>
#include "cursor.h"

typedef enum { NODE_INTERNAL, NODE_LEAF } NodeType;

// Common node header (6 bytes)
#define NODE_TYPE_SIZE          1
#define IS_ROOT_SIZE            1
#define PARENT_POINTER_SIZE     4
#define COMMON_NODE_HEADER_SIZE (NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE)

// Leaf node header (14 bytes total)
#define LEAF_NODE_NUM_CELLS_SIZE   4
#define LEAF_NODE_NEXT_LEAF_SIZE   4
#define LEAF_NODE_NUM_CELLS_OFFSET COMMON_NODE_HEADER_SIZE
#define LEAF_NODE_NEXT_LEAF_OFFSET (COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE)
#define LEAF_NODE_HEADER_SIZE      (COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE + LEAF_NODE_NEXT_LEAF_SIZE)

// Leaf cell: key is always uint32_t (4 bytes); value size = row_size (runtime)
#define LEAF_NODE_KEY_SIZE        4
#define LEAF_NODE_SPACE_FOR_CELLS (PAGE_SIZE - LEAF_NODE_HEADER_SIZE)

// Internal node header / cell
#define INTERNAL_NODE_NUM_KEYS_SIZE    4
#define INTERNAL_NODE_RIGHT_CHILD_SIZE 4
#define INTERNAL_NODE_HEADER_SIZE      (COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE)
#define INTERNAL_NODE_KEY_SIZE         4
#define INTERNAL_NODE_CHILD_SIZE       4
#define INTERNAL_NODE_CELL_SIZE        (INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE)
#define INTERNAL_NODE_MAX_KEYS         ((PAGE_SIZE - INTERNAL_NODE_HEADER_SIZE) / INTERNAL_NODE_CELL_SIZE)
#define INTERNAL_NODE_MAX_CELLS        3
#define INVALID_PAGE_NUM               UINT32_MAX

// Leaf node accessors — cell_size is runtime, so need Table*
uint32_t *leaf_node_num_cells(void *node);
void     *leaf_node_cell(Table *table, void *node, uint32_t cell_num);
uint32_t *leaf_node_key(Table *table, void *node, uint32_t cell_num);
void     *leaf_node_value(Table *table, void *node, uint32_t cell_num);
uint32_t *leaf_node_next_leaf(void *node);
void      initialize_leaf_node(void *node);

// CRUD on leaf
void     leaf_node_insert(Cursor *cursor, uint32_t key, void *row_data);
void     leaf_node_delete(Cursor *cursor);
void     leaf_node_handle_underflow(Table *table, uint32_t page_num);
Cursor  *leaf_node_find(Table *table, uint32_t page_num, uint32_t key);

// Node type / root helpers
NodeType  get_node_type(void *node);
void      set_node_type(void *node, NodeType type);
bool      is_node_root(void *node);
void      set_node_root(void *node, bool is_root);

// Internal node accessors
uint32_t *internal_node_num_keys(void *node);
uint32_t *internal_node_right_child(void *node);
uint32_t *internal_node_child(void *node, uint32_t child_num);
uint32_t *internal_node_key(void *node, uint32_t key_num);
uint32_t *internal_node_cell(void *node, uint32_t cell_num);
void      initialize_internal_node(void *node);

// Split / insert
uint32_t  get_unused_page_num(Pager *pager);
void      leaf_node_split_and_insert(Cursor *cursor, uint32_t key, void *row_data);
void      create_new_root(Table *table, uint32_t right_child_page_num);
void      internal_node_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num);
void      internal_node_split_and_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num);

// Search
Cursor   *internal_node_find(Table *table, uint32_t page_num, uint32_t key);
Cursor   *table_find(Table *table, uint32_t key);

// Debug
void indent(uint32_t level);
void print_tree(Table *table, uint32_t page_num, uint32_t indentation_level);

#endif
