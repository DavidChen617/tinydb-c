#ifndef SCHEMA_H
#define SCHEMA_H

#include <stdint.h>

#define MAX_TABLES      8
#define MAX_COLUMNS     10
#define MAX_COL_NAME    32
#define MAX_TABLE_NAME  32

typedef enum { COL_INT, COL_TEXT } ColType;

typedef struct {
    char     name[MAX_COL_NAME];
    ColType  type;
    uint32_t size;    // INT=4, TEXT=user-specified bytes
    uint32_t offset;  // byte offset within serialized row
} Column;

typedef struct {
    char     name[MAX_TABLE_NAME];
    uint32_t root_page_num;
    uint32_t num_columns;
    uint32_t row_size;
    Column   columns[MAX_COLUMNS];
} TableMeta;

#endif
