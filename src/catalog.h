#ifndef CATALOG_H
#define CATALOG_H

#include "schema.h"
#include "pager.h"

#define CATALOG_PAGE_NUM 0

typedef struct {
    uint32_t  num_tables;
    TableMeta tables[MAX_TABLES];
} Catalog;

void catalog_load(Pager *pager, Catalog *catalog);
void catalog_flush(Pager *pager, Catalog *catalog);
int  catalog_find(Catalog *catalog, const char *name);
int  catalog_add(Catalog *catalog, TableMeta *meta);
void catalog_remove(Catalog *catalog, int idx);

#endif
