#include <stdlib.h>
#include "cursor.h"
#include "btree.h"

Cursor *table_start(Table *table){
    Cursor *cursor = table_find(table, 0);
    void *leaf = get_page(table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(leaf);
    cursor->end_of_table = (num_cells == 0);

    return cursor;
}

Cursor *table_end(Table *table){
    void *root = get_page(table->pager, table->root_page_num);
    uint32_t num_cells = *leaf_node_num_cells(root);

    Cursor *cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = table->root_page_num;
    cursor->cell_num = num_cells;
    cursor->end_of_table = true;

    return cursor;
}

void *cursor_value(Cursor *cursor){
    void *page = get_page(cursor->table->pager, cursor->page_num);
    return leaf_node_value(page, cursor->cell_num);
}

void cursor_advance(Cursor *cursor){
    void *page = get_page(cursor->table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(page);

    cursor->cell_num++;
    if(cursor->cell_num >= num_cells){
        uint32_t next_page = *leaf_node_next_leaf(page);
        if(next_page == 0 || next_page == INVALID_PAGE_NUM){
            cursor->end_of_table = true;
        }
        else{
            cursor->page_num = next_page;
            cursor->cell_num = 0;
        }
    }
}

