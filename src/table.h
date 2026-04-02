#ifndef TABLE_H
#define TABLE_H

#include <stdint.h>
#include "pager.h"

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

#define ID_SIZE 4
#define USERNAME_SIZE 33
#define EMAIL_SIZE 256
#define ROW_SIZE (ID_SIZE + USERNAME_SIZE + EMAIL_SIZE)

#define ID_OFFSET 0
#define USERNAME_OFFSET ID_SIZE
#define EMAIL_OFFSET (ID_SIZE + USERNAME_SIZE)

#define ROWS_PER_PAGE (PAGE_SIZE / ROW_SIZE)
#define TABLE_MAX_ROWS (ROWS_PER_PAGE * MAX_PAGES)

typedef struct
{
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1];
    char email[COLUMN_EMAIL_SIZE + 1];
} Row;

typedef struct
{
    uint32_t root_page_num;
    Pager *pager;
} Table;

void print_row(Row *row);
void serialize_row(Row *src, void *dest);
void deserialize_row(void *src, Row *dest);
void *row_slot(Table *table, uint32_t row_num);
Table *db_open(const char *filename);
void db_close(Table *table);

#endif
