#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "table.h"
#include "btree.h"

static void compute_table_constants(Table *t) {
    t->cell_size         = LEAF_NODE_KEY_SIZE + t->meta->row_size;
    uint32_t space       = PAGE_SIZE - LEAF_NODE_HEADER_SIZE;
    t->max_cells         = space / t->cell_size;
    t->min_cells         = t->max_cells / 2;
    t->right_split_count = (t->max_cells + 1) / 2;
    t->left_split_count  = (t->max_cells + 1) - t->right_split_count;
}

Database *db_open(const char *filename) {
    Database *db = malloc(sizeof(Database));
    db->pager = pager_open(filename);

    if (db->pager->file_length == 0) {
        db->catalog.num_tables = 0;
        catalog_flush(db->pager, &db->catalog);
    } else {
        catalog_load(db->pager, &db->catalog);
    }
    return db;
}

void db_close(Database *db) {
    catalog_flush(db->pager, &db->catalog);
    for (uint32_t i = 0; i < db->pager->num_pages; i++) {
        if (db->pager->pages[i])
            pager_flush(db->pager, i, PAGE_SIZE);
    }
    pager_close(db->pager);
    free(db);
}

Table *table_open(Database *db, const char *name) {
    int idx = catalog_find(&db->catalog, name);
    if (idx < 0) return NULL;

    Table *t   = malloc(sizeof(Table));
    t->pager   = db->pager;
    t->meta    = &db->catalog.tables[idx];
    t->root_page_num = t->meta->root_page_num;
    compute_table_constants(t);
    return t;
}

void table_close(Table *table) {
    free(table);
}

void serialize_row(TableMeta *meta, void **values, void *dest) {
    for (uint32_t i = 0; i < meta->num_columns; i++) {
        Column *col = &meta->columns[i];
        void   *target = (char *)dest + col->offset;
        if (col->type == COL_INT) {
            int32_t v = (int32_t)(intptr_t)values[i];
            memcpy(target, &v, sizeof(int32_t));
        } else {
            strncpy((char *)target, (char *)values[i], col->size - 1);
            ((char *)target)[col->size - 1] = '\0';
        }
    }
}

void deserialize_row(TableMeta *meta, void *src, void **values) {
    for (uint32_t i = 0; i < meta->num_columns; i++) {
        Column *col  = &meta->columns[i];
        void   *field = (char *)src + col->offset;
        values[i] = field;
    }
}

void print_row(TableMeta *meta, void *row_data) {
    printf("(");
    for (uint32_t i = 0; i < meta->num_columns; i++) {
        Column *col   = &meta->columns[i];
        void   *field = (char *)row_data + col->offset;
        if (i > 0) printf(", ");
        if (col->type == COL_INT) {
            int32_t v;
            memcpy(&v, field, sizeof(int32_t));
            printf("%d", v);
        } else {
            printf("%s", (char *)field);
        }
    }
    printf(")\n");
}
