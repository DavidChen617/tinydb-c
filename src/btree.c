#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "btree.h"
#include "cursor.h"

static uint32_t *node_parent(void *node);
static uint32_t *internal_node_cell_ptr(void *node, uint32_t cell_num);

// Offset constants
#define NODE_TYPE_OFFSET                 0
#define IS_ROOT_OFFSET                   NODE_TYPE_SIZE
#define PARENT_POINTER_OFFSET            (IS_ROOT_OFFSET + IS_ROOT_SIZE)
#define LEAF_NODE_NUM_CELLS_OFFSET_LOCAL COMMON_NODE_HEADER_SIZE
#define LEAF_NODE_CELLS_OFFSET           LEAF_NODE_HEADER_SIZE
#define INTERNAL_NODE_NUM_KEYS_OFFSET    COMMON_NODE_HEADER_SIZE
#define INTERNAL_NODE_RIGHT_CHILD_OFFSET (INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE)
#define INTERNAL_NODE_CELLS_OFFSET       INTERNAL_NODE_HEADER_SIZE

// ── Leaf node accessors ───────────────────────────────────────────────────────

uint32_t *leaf_node_num_cells(void *node) {
    return (uint32_t *)((char *)node + LEAF_NODE_NUM_CELLS_OFFSET);
}

void *leaf_node_cell(Table *table, void *node, uint32_t cell_num) {
    return (char *)node + LEAF_NODE_CELLS_OFFSET + cell_num * table->cell_size;
}

uint32_t *leaf_node_key(Table *table, void *node, uint32_t cell_num) {
    return (uint32_t *)leaf_node_cell(table, node, cell_num);
}

void *leaf_node_value(Table *table, void *node, uint32_t cell_num) {
    return (char *)leaf_node_cell(table, node, cell_num) + LEAF_NODE_KEY_SIZE;
}

uint32_t *leaf_node_next_leaf(void *node) {
    return (uint32_t *)((char *)node + LEAF_NODE_NEXT_LEAF_OFFSET);
}

void initialize_leaf_node(void *node) {
    set_node_type(node, NODE_LEAF);
    set_node_root(node, false);
    *leaf_node_num_cells(node) = 0;
    *leaf_node_next_leaf(node) = INVALID_PAGE_NUM;
}

// ── Node type / root ─────────────────────────────────────────────────────────

NodeType get_node_type(void *node) {
    return (NodeType)*((uint8_t *)((char *)node + NODE_TYPE_OFFSET));
}

void set_node_type(void *node, NodeType type) {
    *((uint8_t *)((char *)node + NODE_TYPE_OFFSET)) = (uint8_t)type;
}

bool is_node_root(void *node) {
    return (bool)*((uint8_t *)((char *)node + IS_ROOT_OFFSET));
}

void set_node_root(void *node, bool is_root) {
    *((uint8_t *)((char *)node + IS_ROOT_OFFSET)) = (uint8_t)is_root;
}

// ── Internal node accessors ──────────────────────────────────────────────────

uint32_t *internal_node_num_keys(void *node) {
    return (uint32_t *)((char *)node + INTERNAL_NODE_NUM_KEYS_OFFSET);
}

uint32_t *internal_node_right_child(void *node) {
    return (uint32_t *)((char *)node + INTERNAL_NODE_RIGHT_CHILD_OFFSET);
}

static uint32_t *internal_node_cell_ptr(void *node, uint32_t cell_num) {
    return (uint32_t *)((char *)node + INTERNAL_NODE_CELLS_OFFSET + cell_num * INTERNAL_NODE_CELL_SIZE);
}

uint32_t *internal_node_cell(void *node, uint32_t cell_num) {
    return (uint32_t *)((char *)node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE);
}

uint32_t *internal_node_child(void *node, uint32_t child_num) {
    uint32_t num_keys = *internal_node_num_keys(node);
    if (child_num > num_keys) {
        printf("Error: child_num %d > num_keys %d\n", child_num, num_keys);
        exit(EXIT_FAILURE);
    }
    if (child_num == num_keys) {
        uint32_t *rc = internal_node_right_child(node);
        if (*rc == INVALID_PAGE_NUM) { printf("Error: invalid right child\n"); exit(EXIT_FAILURE); }
        return rc;
    }
    uint32_t *child = internal_node_cell(node, child_num);
    if (*child == INVALID_PAGE_NUM) { printf("Error: invalid child %d\n", child_num); exit(EXIT_FAILURE); }
    return child;
}

uint32_t *internal_node_key(void *node, uint32_t key_num) {
    return (uint32_t *)((char *)internal_node_cell_ptr(node, key_num) + INTERNAL_NODE_CHILD_SIZE);
}

void initialize_internal_node(void *node) {
    set_node_type(node, NODE_INTERNAL);
    set_node_root(node, false);
    *internal_node_num_keys(node) = 0;
    *internal_node_right_child(node) = INVALID_PAGE_NUM;
}

// ── Parent pointer / max key helpers ─────────────────────────────────────────

static uint32_t *node_parent(void *node) {
    return (uint32_t *)((char *)node + PARENT_POINTER_OFFSET);
}

static uint32_t get_node_max_key(Table *table, void *node) {
    if (get_node_type(node) == NODE_LEAF)
        return *leaf_node_key(table, node, *leaf_node_num_cells(node) - 1);
    void *rc = get_page(table->pager, *internal_node_right_child(node));
    return get_node_max_key(table, rc);
}

static uint32_t internal_node_find_child(void *node, uint32_t key) {
    uint32_t num_keys = *internal_node_num_keys(node);
    uint32_t lo = 0, hi = num_keys;
    while (lo != hi) {
        uint32_t mid = (lo + hi) / 2;
        if (*internal_node_key(node, mid) >= key) hi = mid;
        else                                       lo = mid + 1;
    }
    return lo;
}

static void update_internal_node_key(void *parent, uint32_t old_key, uint32_t new_key) {
    uint32_t idx = internal_node_find_child(parent, old_key);
    *internal_node_key(parent, idx) = new_key;
}

// ── Leaf find ────────────────────────────────────────────────────────────────

Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key) {
    void *node = get_page(table->pager, page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    Cursor *cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = page_num;

    uint32_t lo = 0, hi = num_cells;
    while (lo != hi) {
        uint32_t mid = (lo + hi) / 2;
        uint32_t k = *leaf_node_key(table, node, mid);
        if (k == key) { cursor->cell_num = mid; cursor->end_of_table = false; return cursor; }
        if (key < k) hi = mid;
        else         lo = mid + 1;
    }
    cursor->cell_num = lo;
    cursor->end_of_table = (lo == num_cells);
    return cursor;
}

Cursor *internal_node_find(Table *table, uint32_t page_num, uint32_t key) {
    void *node = get_page(table->pager, page_num);
    uint32_t child_page = *internal_node_child(node, internal_node_find_child(node, key));
    void *child = get_page(table->pager, child_page);
    if (get_node_type(child) == NODE_LEAF)
        return leaf_node_find(table, child_page, key);
    return internal_node_find(table, child_page, key);
}

Cursor *table_find(Table *table, uint32_t key) {
    void *root = get_page(table->pager, table->root_page_num);
    if (get_node_type(root) == NODE_LEAF)
        return leaf_node_find(table, table->root_page_num, key);
    return internal_node_find(table, table->root_page_num, key);
}

// ── Leaf insert ──────────────────────────────────────────────────────────────

void leaf_node_insert(Cursor *cursor, uint32_t key, void *row_data) {
    void *node = get_page(cursor->table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    if (num_cells >= cursor->table->max_cells) {
        leaf_node_split_and_insert(cursor, key, row_data);
        return;
    }

    for (uint32_t i = num_cells; i > cursor->cell_num; i--)
        memcpy(leaf_node_cell(cursor->table, node, i),
               leaf_node_cell(cursor->table, node, i - 1),
               cursor->table->cell_size);

    *leaf_node_num_cells(node) += 1;
    *leaf_node_key(cursor->table, node, cursor->cell_num) = key;
    memcpy(leaf_node_value(cursor->table, node, cursor->cell_num),
           row_data, cursor->table->meta->row_size);
}

// ── Leaf delete + rebalancing ────────────────────────────────────────────────

void leaf_node_delete(Cursor *cursor) {
    void *node = get_page(cursor->table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    for (uint32_t i = cursor->cell_num; i < num_cells - 1; i++)
        memcpy(leaf_node_cell(cursor->table, node, i),
               leaf_node_cell(cursor->table, node, i + 1),
               cursor->table->cell_size);
    (*leaf_node_num_cells(node))--;

    if (!is_node_root(node) && *leaf_node_num_cells(node) < cursor->table->min_cells)
        leaf_node_handle_underflow(cursor->table, cursor->page_num);
}

static uint32_t find_child_index_in_parent(void *parent, uint32_t child_page_num) {
    uint32_t num_keys = *internal_node_num_keys(parent);
    for (uint32_t i = 0; i < num_keys; i++)
        if (*internal_node_child(parent, i) == child_page_num) return i;
    return num_keys;
}

static void collapse_root(Table *table) {
    void *root = get_page(table->pager, table->root_page_num);
    uint32_t child_page = *internal_node_right_child(root);
    void *child = get_page(table->pager, child_page);
    memcpy(root, child, PAGE_SIZE);
    set_node_root(root, true);
    if (get_node_type(root) == NODE_INTERNAL) {
        for (uint32_t i = 0; i < *internal_node_num_keys(root); i++) {
            void *c = get_page(table->pager, *internal_node_child(root, i));
            *node_parent(c) = table->root_page_num;
        }
        void *c = get_page(table->pager, *internal_node_right_child(root));
        *node_parent(c) = table->root_page_num;
    }
}

void leaf_node_handle_underflow(Table *table, uint32_t page_num) {
    void *node = get_page(table->pager, page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);
    uint32_t parent_page = *node_parent(node);
    void *parent = get_page(table->pager, parent_page);
    uint32_t num_keys = *internal_node_num_keys(parent);
    uint32_t my_idx = find_child_index_in_parent(parent, page_num);

    // Borrow from right sibling
    if (my_idx < num_keys) {
        uint32_t right_page = *internal_node_child(parent, my_idx + 1);
        void *right = get_page(table->pager, right_page);
        uint32_t right_num = *leaf_node_num_cells(right);
        if (right_num > table->min_cells) {
            memcpy(leaf_node_cell(table, node, num_cells), leaf_node_cell(table, right, 0), table->cell_size);
            (*leaf_node_num_cells(node))++;
            for (uint32_t i = 0; i < right_num - 1; i++)
                memcpy(leaf_node_cell(table, right, i), leaf_node_cell(table, right, i + 1), table->cell_size);
            (*leaf_node_num_cells(right))--;
            *internal_node_key(parent, my_idx) = *leaf_node_key(table, node, num_cells);
            return;
        }
    }

    // Borrow from left sibling
    if (my_idx > 0) {
        uint32_t left_page = *internal_node_child(parent, my_idx - 1);
        void *left = get_page(table->pager, left_page);
        uint32_t left_num = *leaf_node_num_cells(left);
        if (left_num > table->min_cells) {
            for (uint32_t i = num_cells; i > 0; i--)
                memcpy(leaf_node_cell(table, node, i), leaf_node_cell(table, node, i - 1), table->cell_size);
            memcpy(leaf_node_cell(table, node, 0), leaf_node_cell(table, left, left_num - 1), table->cell_size);
            (*leaf_node_num_cells(node))++;
            (*leaf_node_num_cells(left))--;
            *internal_node_key(parent, my_idx - 1) = *leaf_node_key(table, left, left_num - 2);
            return;
        }
    }

    // Merge
    if (my_idx < num_keys) {
        uint32_t right_page = *internal_node_child(parent, my_idx + 1);
        void *right = get_page(table->pager, right_page);
        uint32_t right_num = *leaf_node_num_cells(right);
        for (uint32_t i = 0; i < right_num; i++)
            memcpy(leaf_node_cell(table, node, num_cells + i), leaf_node_cell(table, right, i), table->cell_size);
        *leaf_node_num_cells(node) += right_num;
        *leaf_node_next_leaf(node) = *leaf_node_next_leaf(right);
        if (my_idx + 1 == num_keys) {
            *internal_node_right_child(parent) = page_num;
        } else {
            *internal_node_key(parent, my_idx) = *internal_node_key(parent, my_idx + 1);
            for (uint32_t i = my_idx + 1; i < num_keys - 1; i++)
                memcpy(internal_node_cell_ptr(parent, i), internal_node_cell_ptr(parent, i + 1), INTERNAL_NODE_CELL_SIZE);
        }
    } else {
        uint32_t left_page = *internal_node_child(parent, num_keys - 1);
        void *left = get_page(table->pager, left_page);
        uint32_t left_num = *leaf_node_num_cells(left);
        for (uint32_t i = 0; i < num_cells; i++)
            memcpy(leaf_node_cell(table, left, left_num + i), leaf_node_cell(table, node, i), table->cell_size);
        *leaf_node_num_cells(left) += num_cells;
        *leaf_node_next_leaf(left) = *leaf_node_next_leaf(node);
        *internal_node_right_child(parent) = left_page;
    }
    (*internal_node_num_keys(parent))--;

    if (is_node_root(parent) && *internal_node_num_keys(parent) == 0)
        collapse_root(table);
}

// ── Split / create root ──────────────────────────────────────────────────────

uint32_t get_unused_page_num(Pager *pager) { return pager->num_pages; }

void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, void *row_data) {
    void *old_node = get_page(cursor->table->pager, cursor->page_num);
    uint32_t old_max = get_node_max_key(cursor->table, old_node);
    uint32_t new_page = get_unused_page_num(cursor->table->pager);
    void *new_node = get_page(cursor->table->pager, new_page);
    initialize_leaf_node(new_node);

    uint32_t max_cells  = cursor->table->max_cells;
    uint32_t left_split = cursor->table->left_split_count;

    for (int32_t i = (int32_t)max_cells; i >= 0; i--) {
        void *dest_node;
        uint32_t idx;
        if (i >= (int32_t)left_split) { dest_node = new_node; idx = (uint32_t)i - left_split; }
        else                          { dest_node = old_node; idx = (uint32_t)i; }

        if (i == (int32_t)cursor->cell_num) {
            *leaf_node_key(cursor->table, dest_node, idx) = key;
            memcpy(leaf_node_value(cursor->table, dest_node, idx), row_data, cursor->table->meta->row_size);
        } else if (i > (int32_t)cursor->cell_num) {
            memcpy(leaf_node_cell(cursor->table, dest_node, idx),
                   leaf_node_cell(cursor->table, old_node, (uint32_t)i - 1),
                   cursor->table->cell_size);
        } else {
            memcpy(leaf_node_cell(cursor->table, dest_node, idx),
                   leaf_node_cell(cursor->table, old_node, (uint32_t)i),
                   cursor->table->cell_size);
        }
    }

    *leaf_node_num_cells(old_node) = left_split;
    *leaf_node_num_cells(new_node) = cursor->table->right_split_count;
    uint32_t old_next = *leaf_node_next_leaf(old_node);
    *leaf_node_next_leaf(old_node) = new_page;
    *leaf_node_next_leaf(new_node) = old_next;

    if (is_node_root(old_node)) {
        create_new_root(cursor->table, new_page);
    } else {
        uint32_t parent_page = *node_parent(old_node);
        uint32_t new_max = get_node_max_key(cursor->table, old_node);
        void *parent = get_page(cursor->table->pager, parent_page);
        update_internal_node_key(parent, old_max, new_max);
        *node_parent(new_node) = parent_page;
        internal_node_insert(cursor->table, parent_page, new_page);
    }
}

void create_new_root(Table *table, uint32_t right_child_page_num) {
    void *root = get_page(table->pager, table->root_page_num);
    void *right_child = get_page(table->pager, right_child_page_num);
    uint32_t left_child_page_num = get_unused_page_num(table->pager);
    void *left_child = get_page(table->pager, left_child_page_num);

    memcpy(left_child, root, PAGE_SIZE);
    set_node_root(left_child, false);
    *node_parent(left_child) = table->root_page_num;

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

    initialize_internal_node(root);
    set_node_root(root, true);
    *internal_node_num_keys(root) = 1;
    *internal_node_child(root, 0) = left_child_page_num;
    *internal_node_key(root, 0) = get_node_max_key(table, left_child);
    *internal_node_right_child(root) = right_child_page_num;
    *node_parent(right_child) = table->root_page_num;
}

void internal_node_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num) {
    void *parent = get_page(table->pager, parent_page_num);
    void *child = get_page(table->pager, child_page_num);
    uint32_t child_max = get_node_max_key(table, child);
    uint32_t idx = internal_node_find_child(parent, child_max);
    uint32_t orig_keys = *internal_node_num_keys(parent);

    if (orig_keys >= INTERNAL_NODE_MAX_KEYS) {
        internal_node_split_and_insert(table, parent_page_num, child_page_num);
        return;
    }

    uint32_t rc_page = *internal_node_right_child(parent);
    if (rc_page == INVALID_PAGE_NUM) {
        *internal_node_right_child(parent) = child_page_num;
        return;
    }

    void *right_child = get_page(table->pager, rc_page);
    *internal_node_num_keys(parent) = orig_keys + 1;

    if (child_max > get_node_max_key(table, right_child)) {
        *internal_node_child(parent, orig_keys) = rc_page;
        *internal_node_key(parent, orig_keys) = get_node_max_key(table, right_child);
        *internal_node_right_child(parent) = child_page_num;
        *node_parent(child) = parent_page_num;
    } else {
        for (uint32_t i = orig_keys; i > idx; i--)
            memcpy(internal_node_cell_ptr(parent, i), internal_node_cell_ptr(parent, i - 1), INTERNAL_NODE_CELL_SIZE);
        *internal_node_child(parent, idx) = child_page_num;
        *internal_node_key(parent, idx) = child_max;
        *node_parent(child) = parent_page_num;
    }
}

void internal_node_split_and_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num) {
    uint32_t old_page_num = parent_page_num;
    void *old_node = get_page(table->pager, parent_page_num);
    uint32_t old_max = get_node_max_key(table, old_node);
    void *child = get_page(table->pager, child_page_num);
    uint32_t child_max = get_node_max_key(table, child);
    uint32_t new_page_num = get_unused_page_num(table->pager);
    uint32_t splitting_root = is_node_root(old_node);

    void *parent, *new_node;
    if (splitting_root) {
        create_new_root(table, new_page_num);
        parent = get_page(table->pager, table->root_page_num);
        old_page_num = *internal_node_child(parent, 0);
        old_node = get_page(table->pager, old_page_num);
        new_node = get_page(table->pager, new_page_num);
    } else {
        parent = get_page(table->pager, *node_parent(old_node));
        new_node = get_page(table->pager, new_page_num);
        initialize_internal_node(new_node);
    }

    uint32_t *old_num_keys = internal_node_num_keys(old_node);
    uint32_t cur_page = *internal_node_right_child(old_node);
    void *cur = get_page(table->pager, cur_page);

    internal_node_insert(table, new_page_num, cur_page);
    *node_parent(cur) = new_page_num;
    *internal_node_right_child(old_node) = INVALID_PAGE_NUM;

    for (int i = INTERNAL_NODE_MAX_KEYS - 1; i > INTERNAL_NODE_MAX_KEYS / 2; i--) {
        cur_page = *internal_node_child(old_node, (uint32_t)i);
        cur = get_page(table->pager, cur_page);
        internal_node_insert(table, new_page_num, cur_page);
        *node_parent(cur) = new_page_num;
        (*old_num_keys)--;
    }

    *internal_node_right_child(old_node) = *internal_node_child(old_node, *old_num_keys - 1);
    (*old_num_keys)--;

    uint32_t max_after_split = get_node_max_key(table, old_node);
    uint32_t dest = child_max < max_after_split ? old_page_num : new_page_num;
    internal_node_insert(table, dest, child_page_num);
    *node_parent(child) = dest;

    update_internal_node_key(parent, old_max, get_node_max_key(table, old_node));

    if (!splitting_root) {
        internal_node_insert(table, *node_parent(old_node), new_page_num);
        *node_parent(new_node) = *node_parent(old_node);
    }
}

// ── Debug print ──────────────────────────────────────────────────────────────

void indent(uint32_t level) {
    for (uint32_t i = 0; i < level; i++) printf(" ");
}

void print_tree(Table *table, uint32_t page_num, uint32_t indentation_level) {
    void *node = get_page(table->pager, page_num);
    uint32_t num_keys, child;
    switch (get_node_type(node)) {
    case NODE_LEAF:
        num_keys = *leaf_node_num_cells(node);
        indent(indentation_level);
        printf("- leaf (size %d)\n", num_keys);
        for (uint32_t i = 0; i < num_keys; i++) {
            indent(indentation_level + 1);
            printf("- %d\n", *leaf_node_key(table, node, i));
        }
        break;
    case NODE_INTERNAL:
        num_keys = *internal_node_num_keys(node);
        indent(indentation_level);
        printf("- internal (size %d)\n", num_keys);
        if (num_keys > 0) {
            for (uint32_t i = 0; i < num_keys; i++) {
                child = *internal_node_child(node, i);
                print_tree(table, child, indentation_level + 1);
                indent(indentation_level + 1);
                printf("- key %d\n", *internal_node_key(node, i));
            }
            child = *internal_node_right_child(node);
            print_tree(table, child, indentation_level + 1);
        }
        break;
    }
}
