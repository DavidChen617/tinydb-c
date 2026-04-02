#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "table.h"
#include "btree.h"

void print_row(Row *row)
{
    printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

void serialize_row(Row *src, void *dest)
{
    memcpy(dest + ID_OFFSET, &src->id, ID_SIZE);
    memcpy(dest + USERNAME_OFFSET, &src->username, USERNAME_SIZE);
    memcpy(dest + EMAIL_OFFSET, &src->email, EMAIL_SIZE);
}

void deserialize_row(void *src, Row *dest)
{
    memcpy(&dest->id, src + ID_OFFSET, ID_SIZE);
    memcpy(dest->username, src + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&dest->email, src + EMAIL_OFFSET, EMAIL_SIZE);
}

void *row_slot(Table *table, uint32_t row_num){
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    void *page = get_page(table->pager, page_num);
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    return page + row_offset * ROW_SIZE;
}

Table *db_open(const char *filename){
    Pager *pager = pager_open(filename);
    Table *table = malloc(sizeof(Table));
    table->pager = pager;
    table->root_page_num = 0;

    if(pager->file_length == 0){
        void *root = get_page(pager, 0);
        initialize_leaf_node(root);
        set_node_root(root, true);
    }

    return table;
}

void db_close(Table *table){
    Pager *pager = table->pager;


    for (uint32_t i = 0; i < pager->num_pages; i++)
    {
        if(pager->pages[i])
            pager_flush(pager, i, PAGE_SIZE);
    }

    pager_close(pager);
    free(table);
}
