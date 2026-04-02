#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "btree.h"
#include "cursor.h"

// offset contant
#define NODE_TYPE_OFFSET 0
#define IS_ROOT_OFFSET NODE_TYPE_SIZE
#define PARENT_POINTER_OFFSET (IS_ROOT_OFFSET + IS_ROOT_SIZE)
#define LEAF_NODE_NUM_CELLS_OFFSET COMMON_NODE_HEADER_SIZE
#define LEAF_NODE_CELLS_OFFSET LEAF_NODE_HEADER_SIZE

// internal node access
#define INTERNAL_NODE_NUM_KEYS_OFFSET COMMON_NODE_HEADER_SIZE
#define INTERNAL_NODE_RIGHT_CHILD_OFFSET (INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE)
#define INTERNAL_NODE_CELLS_OFFSET INTERNAL_NODE_HEADER_SIZE

uint32_t *leaf_node_num_cells(void *node)
{
    return (uint32_t *)((char *)node + LEAF_NODE_NUM_CELLS_OFFSET);
}

void *leaf_node_cell(void *node, uint32_t cell_num)
{
    return (char *)node + LEAF_NODE_CELLS_OFFSET + cell_num * LEAF_NODE_CELL_SIZE;
}

uint32_t *leaf_node_key(void *node, uint32_t cell_num)
{
    return (uint32_t *)leaf_node_cell(node, cell_num);
}

void *leaf_node_value(void *node, uint32_t cell_num)
{
    return (char *)leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
}

void initialize_leaf_node(void *node)
{
    set_node_type(node, NODE_LEAF);
    set_node_root(node, false);
    *leaf_node_num_cells(node) = 0;
    *leaf_node_next_leaf(node) = INVALID_PAGE_NUM;
}

void leaf_node_insert(Cursor *cursor, uint32_t key, Row *value)
{
    void *node = get_page(cursor->table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    if (num_cells >= LEAF_NODE_MAX_CELLS)
    {
        leaf_node_split_and_insert(cursor, key, value);
        return;
    }

    // 把 cursor 位置之後的 cell 往後移，騰出空間（維持排序）
    for (uint32_t i = num_cells; i > cursor->cell_num; i--)
    {
        memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1), LEAF_NODE_CELL_SIZE);
    }

    *leaf_node_num_cells(node) += 1;
    *leaf_node_key(node, cursor->cell_num) = key;
    serialize_row(value, leaf_node_value(node, cursor->cell_num));
}

Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key)
{
    void *node = get_page(table->pager, page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    Cursor *cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = page_num;

    // binary search
    uint32_t min_index = 0;
    uint32_t one_past_max = num_cells;

    while (min_index != one_past_max)
    {
        uint32_t index = (min_index + one_past_max) / 2;
        uint32_t key_at_index = *leaf_node_key(node, index);

        if (key == key_at_index)
        {
            cursor->cell_num = index;
            cursor->end_of_table = false;
            return cursor;
        }
        if (key < key_at_index)
        {
            one_past_max = index;
        }
        else
        {
            min_index = index + 1;
        }
    }

    cursor->cell_num = min_index;
    cursor->end_of_table = (min_index == num_cells);

    return cursor;
}

// node type / root access
NodeType get_node_type(void *node)
{
    return (NodeType) * ((uint8_t *)((char *)node + NODE_TYPE_OFFSET));
}

void set_node_type(void *node, NodeType type)
{
    *((uint8_t *)((char *)node + NODE_TYPE_OFFSET)) = (uint8_t)type;
}

bool is_node_root(void *node)
{
    return (bool)*((uint8_t *)((char *)node + IS_ROOT_OFFSET));
}

void set_node_root(void *node, bool is_root)
{
    *((uint8_t *)((char *)node + IS_ROOT_OFFSET)) = (uint8_t)is_root;
}

// internal node access
uint32_t *internal_node_num_keys(void *node)
{
    return (uint32_t *)((char *)node + INTERNAL_NODE_NUM_KEYS_OFFSET);
}

uint32_t *internal_node_right_child(void *node)
{
    return (uint32_t *)((char *)node + INTERNAL_NODE_RIGHT_CHILD_OFFSET);
}

static uint32_t *internal_node_cell_ptr(void *node, uint32_t cell_num)
{
    return (uint32_t *)((char *)node + INTERNAL_NODE_CELLS_OFFSET + cell_num * INTERNAL_NODE_CELL_SIZE);
}

uint32_t *internal_node_cell(void *node, uint32_t cell_num)
{
    return node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE;
}

uint32_t *internal_node_child(void *node, uint32_t child_num)
{
    uint32_t num_keys = *internal_node_num_keys(node);

    if (child_num > num_keys)
    {
        printf("Tried to access child_num %d > nums %d\n", child_num, num_keys);
        exit(EXIT_FAILURE);
    }
    else if (child_num == num_keys)
    {
        uint32_t *right_child = internal_node_right_child(node);
        if (*right_child == INVALID_PAGE_NUM)
        {
            printf("Tried to access right child of node, but was invalid page\n");
            exit(EXIT_FAILURE);
        }

        return right_child;
    }
    uint32_t *child = internal_node_cell(node, child_num);
    if (*child == INVALID_PAGE_NUM)
    {
        printf("Tried to access chuld %d of node, but was invalid page\n", child_num);
        exit(EXIT_FAILURE);
    }
    return child;
}

uint32_t *internal_node_key(void *node, uint32_t key_num)
{
    return (uint32_t *)((char *)internal_node_cell_ptr(node, key_num) + INTERNAL_NODE_CHILD_SIZE);
}

void initialize_internal_node(void *node)
{
    set_node_type(node, NODE_INTERNAL);
    set_node_root(node, false);
    *internal_node_num_keys(node) = 0;
    *internal_node_right_child(node) = INVALID_PAGE_NUM;
}

// parent pointer accessor
static uint32_t *node_parent(void *node)
{
    return (uint32_t *)((char *)node + PARENT_POINTER_OFFSET);
}

// max key stored in a node
static uint32_t get_node_max_key(Pager *pager, void *node)
{
    if (get_node_type(node) == NODE_LEAF)
        return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);

    void *right_child = get_page(pager, *internal_node_right_child(node));

    return get_node_max_key(pager, right_child);
}

// binary search: which child index should contain key
static uint32_t internal_node_find_child(void *node, uint32_t key)
{
    uint32_t num_keys = *internal_node_num_keys(node);
    uint32_t min_index = 0;
    uint32_t max_index = num_keys;
    while (min_index != max_index)
    {
        uint32_t mid = (min_index + max_index) / 2;
        if (*internal_node_key(node, mid) >= key)
            max_index = mid;
        else
            min_index = mid + 1;
    }
    return min_index;
}

// 更新 parent 中 old_key → new_key
static void update_internal_node_key(void *parent, uint32_t old_key, uint32_t new_key)
{
    uint32_t index = internal_node_find_child(parent, old_key);
    *internal_node_key(parent, index) = new_key;
}

// spilt
uint32_t get_unused_page_num(Pager *pager)
{
    return pager->num_pages;
}

void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, Row *value)
{
    void *old_node = get_page(cursor->table->pager, cursor->page_num);
    uint32_t old_max_key = get_node_max_key(cursor->table->pager, old_node);
    uint32_t new_page_num = get_unused_page_num(cursor->table->pager);
    void *new_node = get_page(cursor->table->pager, new_page_num);
    initialize_leaf_node(new_node);

    // 把 LEAF_NODE_MAX_CELLS + 1 個 cell 分配到兩個 node
    for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--)
    {
        void *dest_node;
        uint32_t index_within_node;

        if (i >= (int32_t)LEAF_NODE_LEFT_SPLIT_COUNT)
        {
            dest_node = new_node;
            index_within_node = i - LEAF_NODE_LEFT_SPLIT_COUNT;
        }
        else
        {
            dest_node = old_node;
            index_within_node = i;
        }

        if (i == (int32_t)cursor->cell_num)
        {
            *leaf_node_key(dest_node, index_within_node) = key;
            serialize_row(value, leaf_node_value(dest_node, index_within_node));
        }
        else if (i > (int32_t)cursor->cell_num)
        {
            memcpy(leaf_node_cell(dest_node, index_within_node), leaf_node_cell(old_node, i - 1),
                   LEAF_NODE_CELL_SIZE);
        }
        else
        {
            memcpy(leaf_node_cell(dest_node, index_within_node), leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
        }
    }

    *leaf_node_num_cells(old_node) = LEAF_NODE_LEFT_SPLIT_COUNT;
    *leaf_node_num_cells(new_node) = LEAF_NODE_RIGHT_SPLIT_COUNT;
    uint32_t old_next = *leaf_node_next_leaf(old_node);
    *leaf_node_next_leaf(old_node) = new_page_num;
    *leaf_node_next_leaf(new_node) = old_next;

    if (is_node_root(old_node))
    {
        create_new_root(cursor->table, new_page_num);
    }
    else
    {
        uint32_t parent_page_num = *node_parent(old_node);
        uint32_t new_max = get_node_max_key(cursor->table->pager, old_node);
        void *parent = get_page(cursor->table->pager, parent_page_num);
        update_internal_node_key(parent, old_max_key, new_max);
        *node_parent(new_node) = parent_page_num;
        internal_node_insert(cursor->table, parent_page_num, new_page_num);
    }
}

void create_new_root(Table *table, uint32_t right_child_page_num)
{
    void *root = get_page(table->pager, table->root_page_num);
    void* right_child = get_page(table->pager, right_child_page_num);
    uint32_t left_child_page_num = get_unused_page_num(table->pager);
    void *left_child = get_page(table->pager, left_child_page_num);

    // 把舊的 root 複製到 left child（先 copy，再判斷 type）
    memcpy(left_child, root, PAGE_SIZE);
    set_node_root(left_child, false);
    *node_parent(left_child) = table->root_page_num;

    // 如果舊 root 是 internal node，需更新其 children 的 parent pointer
    if (get_node_type(left_child) == NODE_INTERNAL) {
        void *child;
        for (uint32_t i = 0; i < *internal_node_num_keys(left_child); i++) {
            child = get_page(table->pager, *internal_node_child(left_child, i));
            *node_parent(child) = left_child_page_num;
        }
        child = get_page(table->pager, *internal_node_right_child(left_child));
        *node_parent(child) = left_child_page_num;
        initialize_internal_node(right_child);
    }

    // root 變成 internal node
    initialize_internal_node(root);
    set_node_root(root, true);
    *internal_node_num_keys(root) = 1;
    *internal_node_child(root, 0) = left_child_page_num;

    uint32_t left_max_key = get_node_max_key(table->pager, left_child);

    *internal_node_key(root, 0) = left_max_key;
    *internal_node_right_child(root) = right_child_page_num;

    *node_parent(right_child) = table->root_page_num;
}

void internal_node_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num)
{
    void *parent = get_page(table->pager, parent_page_num);
    void *child = get_page(table->pager, child_page_num);
    uint32_t child_max_key = get_node_max_key(table->pager, child);
    uint32_t index = internal_node_find_child(parent, child_max_key);
    uint32_t original_num_keys = *internal_node_num_keys(parent);

    if (original_num_keys >= INTERNAL_NODE_MAX_KEYS)
    {
        internal_node_split_and_insert(table, parent_page_num, child_page_num);
        return;
    }

    uint32_t right_child_page_num = *internal_node_right_child(parent);
    if (right_child_page_num == INVALID_PAGE_NUM)
    {
        *internal_node_right_child(parent) = child_page_num;
        return;
    }

    void *right_child = get_page(table->pager, right_child_page_num);

    *internal_node_num_keys(parent) = original_num_keys + 1;

    if (child_max_key > get_node_max_key(table->pager, right_child))
    {
        // 新 child 變成新的 right child，舊的 right child 降為一般 child
        *internal_node_child(parent, original_num_keys) = right_child_page_num;
        *internal_node_key(parent, original_num_keys) = get_node_max_key(table->pager, right_child);
        *internal_node_right_child(parent) = child_page_num;
        *node_parent(child) = parent_page_num;
    }
    else
    {
        // 把 index 之後的 cell 往右移
        for (uint32_t i = original_num_keys; i > index; i--)
        {
            memcpy(internal_node_cell_ptr(parent, i),
                   internal_node_cell_ptr(parent, i - 1),
                   INTERNAL_NODE_CELL_SIZE);
        }
        *internal_node_child(parent, index) = child_page_num;
        *internal_node_key(parent, index) = child_max_key;
        *node_parent(child) = parent_page_num;
    }
}

uint32_t *leaf_node_next_leaf(void *node)
{
    return (uint32_t *)((char *)node + LEAF_NODE_NEXT_LEAF_OFFSET);
}

Cursor *internal_node_find(Table *table, uint32_t page_num, uint32_t key)
{
    void *node = get_page(table->pager, page_num);
    uint32_t num_keys = *internal_node_num_keys(node);

    uint32_t min_index = 0;
    uint32_t max_index = num_keys;

    while (min_index != max_index)
    {
        uint32_t index = (min_index + max_index) / 2;
        uint32_t key_to_right = *internal_node_key(node, index);
        if (key_to_right >= key)
        {
            max_index = index;
        }
        else
        {
            min_index = index + 1;
        }
    }

    uint32_t child_page = *internal_node_child(node, min_index);
    void *child = get_page(table->pager, child_page);

    if (get_node_type(child) == NODE_LEAF)
    {
        return leaf_node_find(table, child_page, key);
    }

    return internal_node_find(table, child_page, key);
}

Cursor *table_find(Table *table, uint32_t key)
{
    void *root = get_page(table->pager, table->root_page_num);
    if (get_node_type(root) == NODE_LEAF)
    {
        return leaf_node_find(table, table->root_page_num, key);
    }

    return internal_node_find(table, table->root_page_num, key);
}

void indent(uint32_t level)
{
    for (uint32_t i = 0; i < level; i++)
    {
        printf(" ");
    }
}

void print_tree(Pager *pager, uint32_t page_num, uint32_t indentation_level)
{
    void *node = get_page(pager, page_num);
    uint32_t num_keys, child;
    switch (get_node_type(node))
    {
    case (NODE_LEAF):
        num_keys = *leaf_node_num_cells(node);
        indent(indentation_level);
        printf("- leaf (size %d)\n", num_keys);
        for (uint32_t i = 0; i < num_keys; i++)
        {
            indent(indentation_level + 1);
            printf("- %d\n", *leaf_node_key(node, i));
        }
        break;
    case (NODE_INTERNAL):
        num_keys = *internal_node_num_keys(node);
        indent(indentation_level);
        printf("- internal (size %d)\n", num_keys);
        if (num_keys > 0)
        {
            for (uint32_t i = 0; i < num_keys; i++)
            {
                child = *internal_node_child(node, i);
                print_tree(pager, child, indentation_level + 1);

                indent(indentation_level + 1);
                printf("- key %d\n", *internal_node_key(node, i));
            }
            child = *internal_node_right_child(node);
            print_tree(pager, child, indentation_level + 1);
        }
        break;
    }
}

void internal_node_split_and_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num)
{
    uint32_t old_page_num = parent_page_num;
    void* old_node = get_page(table->pager, parent_page_num);
    uint32_t old_max = get_node_max_key(table->pager, old_node);
    void* child = get_page(table->pager, child_page_num);
    uint32_t child_max = get_node_max_key(table->pager, child);
    uint32_t new_page_num = get_unused_page_num(table->pager);
    uint32_t splitting_root = is_node_root(old_node);
    
    void *parent;
    void *new_node;
    if (splitting_root) {
        create_new_root(table, new_page_num);
        parent = get_page(table->pager, table->root_page_num);
        old_page_num = *internal_node_child(parent, 0);
        old_node = get_page(table->pager, old_page_num);   // fix: 舊 root 複製到 left child
        new_node = get_page(table->pager, new_page_num);   // fix: splitting_root 也需要 new_node
    } else {
        parent = get_page(table->pager, *node_parent(old_node));
        new_node = get_page(table->pager, new_page_num);
        initialize_internal_node(new_node);
    }

    uint32_t *old_num_keys = internal_node_num_keys(old_node);
    uint32_t cur_page_num = *internal_node_right_child(old_node);
    void *cur = get_page(table->pager, cur_page_num);

    internal_node_insert(table, new_page_num, cur_page_num);
    *node_parent(cur) = new_page_num;
    *internal_node_right_child(old_node) = INVALID_PAGE_NUM;

    // fix: 從 INTERNAL_NODE_MAX_KEYS-1 開始（不是 MAX_KEYS，那是 right_child）
    for (int i = INTERNAL_NODE_MAX_KEYS - 1; i > INTERNAL_NODE_MAX_KEYS / 2; i--)
    {
        cur_page_num = *internal_node_child(old_node, i);
        cur = get_page(table->pager, cur_page_num);

        internal_node_insert(table, new_page_num, cur_page_num);
        *node_parent(cur) = new_page_num;
        (*old_num_keys)--;
    }
    

    *internal_node_right_child(old_node) = *internal_node_child(old_node, *old_num_keys - 1);
    (*old_num_keys)--;   

    uint32_t max_after_split = get_node_max_key(table->pager, old_node);
    uint32_t destination_page_num = child_max < max_after_split ? old_page_num : new_page_num;

    internal_node_insert(table, destination_page_num, child_page_num);
    *node_parent(child) = destination_page_num;

    update_internal_node_key(parent, old_max, get_node_max_key(table->pager, old_node));

    if(!splitting_root){
        internal_node_insert(table, *node_parent(old_node), new_page_num);
        *node_parent(new_node) = *node_parent(old_node);
    }
}
