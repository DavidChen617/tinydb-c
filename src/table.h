#ifndef TABLE_H
#define TABLE_H

#include <stdint.h>
#include "pager.h"
#include "schema.h"
#include "catalog.h"

typedef struct {
    Pager   *pager;
    Catalog  catalog;
} Database;

typedef struct {
    uint32_t   root_page_num;
    Pager     *pager;
    TableMeta *meta;
    // runtime btree layout (computed from meta->row_size)
    uint32_t cell_size;
    uint32_t max_cells;
    uint32_t min_cells;
    uint32_t left_split_count;
    uint32_t right_split_count;
} Table;

Database *db_open(const char *filename);
void      db_close(Database *db);
Table    *table_open(Database *db, const char *name);
void      table_close(Table *table);

void serialize_row(TableMeta *meta, void **values, void *dest);
void deserialize_row(TableMeta *meta, void *src, void **values);
void print_row(TableMeta *meta, void *row_data);

#endif
