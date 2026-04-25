#include <string.h>
#include "catalog.h"

void catalog_load(Pager *pager, Catalog *catalog) {
    void *page = get_page(pager, CATALOG_PAGE_NUM);
    memcpy(catalog, page, sizeof(Catalog));
}

void catalog_flush(Pager *pager, Catalog *catalog) {
    void *page = get_page(pager, CATALOG_PAGE_NUM);
    memcpy(page, catalog, sizeof(Catalog));
}

int catalog_find(Catalog *catalog, const char *name) {
    for (uint32_t i = 0; i < catalog->num_tables; i++) {
        if (strcmp(catalog->tables[i].name, name) == 0) return (int)i;
    }
    return -1;
}

int catalog_add(Catalog *catalog, TableMeta *meta) {
    if (catalog->num_tables >= MAX_TABLES) return -1;
    catalog->tables[catalog->num_tables++] = *meta;
    return (int)(catalog->num_tables - 1);
}

void catalog_remove(Catalog *catalog, int idx) {
    for (uint32_t i = (uint32_t)idx; i < catalog->num_tables - 1; i++)
        catalog->tables[i] = catalog->tables[i + 1];
    catalog->num_tables--;
}
