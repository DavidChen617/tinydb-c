#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "table.h"
#include "cursor.h"
#include "btree.h"

#define INPUT_BUFFER_SIZE 1024
#define MAX_VALUE_LEN     512

typedef struct {
    char buffer[INPUT_BUFFER_SIZE];
} InputBuffer;

void read_input(InputBuffer *input) {
    if (fgets(input->buffer, INPUT_BUFFER_SIZE, stdin) == NULL) {
        printf("Error reading input\n");
        exit(EXIT_FAILURE);
    }
    int len = strlen(input->buffer);
    if (len > 0 && input->buffer[len - 1] == '\n')
        input->buffer[len - 1] = '\0';
}

// ── Schema parsing ────────────────────────────────────────────────────────────

// Parse "name INT" or "name TEXT(n)" into Column. Returns 1 on success.
static int parse_column(const char *token, Column *col) {
    char name[MAX_COL_NAME], type_str[64];
    if (sscanf(token, " %31[^( ] %63s", name, type_str) < 2) return 0;

    strncpy(col->name, name, MAX_COL_NAME - 1);
    col->name[MAX_COL_NAME - 1] = '\0';

    if (strcasecmp(type_str, "INT") == 0) {
        col->type = COL_INT;
        col->size = 4;
        return 1;
    }
    uint32_t text_size = 255;
    if (strncasecmp(type_str, "TEXT", 4) == 0) {
        sscanf(type_str, "TEXT(%u)", &text_size);
        col->type = COL_TEXT;
        col->size = text_size + 1; // +1 for null terminator
        return 1;
    }
    return 0;
}

// Parse "create table name (col1 type, col2 type, ...)" → fill TableMeta
static int parse_create_table(const char *input, TableMeta *meta) {
    char name[MAX_TABLE_NAME];
    char cols_str[INPUT_BUFFER_SIZE];

    const char *paren_open  = strchr(input, '(');
    const char *paren_close = strrchr(input, ')'); // last ')' to handle TEXT(n)
    if (!paren_open || !paren_close || paren_close <= paren_open) {
        printf("Syntax: create table <name> (<col> <type>, ...)\n");
        return 0;
    }

    // Extract table name (between "create table " and '(')
    const char *name_start = input + 13; // skip "create table "
    while (*name_start == ' ') name_start++;
    const char *name_end = paren_open;
    while (name_end > name_start && *(name_end - 1) == ' ') name_end--;
    size_t name_len = (size_t)(name_end - name_start);
    if (name_len >= MAX_TABLE_NAME) name_len = MAX_TABLE_NAME - 1;
    strncpy(name, name_start, name_len);
    name[name_len] = '\0';

    // Extract column list between '(' and last ')'
    size_t cols_len = (size_t)(paren_close - paren_open - 1);
    if (cols_len >= INPUT_BUFFER_SIZE) cols_len = INPUT_BUFFER_SIZE - 1;
    strncpy(cols_str, paren_open + 1, cols_len);
    cols_str[cols_len] = '\0';

    if (sscanf("", "") < 0) {} // suppress unused warning

    strncpy(meta->name, name, MAX_TABLE_NAME - 1);
    meta->name[MAX_TABLE_NAME - 1] = '\0';
    meta->num_columns = 0;
    meta->row_size = 0;

    char *tok = strtok(cols_str, ",");
    while (tok && meta->num_columns < MAX_COLUMNS) {
        Column col = {0};
        if (!parse_column(tok, &col)) {
            printf("Invalid column definition: %s\n", tok);
            return 0;
        }
        col.offset = meta->row_size;
        meta->row_size += col.size;
        meta->columns[meta->num_columns++] = col;
        tok = strtok(NULL, ",");
    }

    if (meta->num_columns == 0) { printf("No columns defined.\n"); return 0; }
    if (meta->columns[0].type != COL_INT) {
        printf("First column must be INT (primary key).\n");
        return 0;
    }
    return 1;
}

// ── Row serialization ─────────────────────────────────────────────────────────

// Parse INSERT values from space-separated tokens into row_buffer
static int build_row(TableMeta *meta, char **tokens, int num_tokens, char *row_buffer) {
    if (num_tokens < (int)meta->num_columns) {
        printf("Expected %d values, got %d\n", meta->num_columns, num_tokens);
        return 0;
    }
    memset(row_buffer, 0, meta->row_size);
    for (uint32_t i = 0; i < meta->num_columns; i++) {
        Column *col = &meta->columns[i];
        void *dest = row_buffer + col->offset;
        if (col->type == COL_INT) {
            int32_t v = atoi(tokens[i]);
            memcpy(dest, &v, sizeof(int32_t));
        } else {
            strncpy((char *)dest, tokens[i], col->size - 1);
        }
    }
    return 1;
}

// ── REPL helpers ──────────────────────────────────────────────────────────────

static void do_select(Table *table) {
    Cursor *cursor = table_start(table);
    while (!cursor->end_of_table) {
        print_row(table->meta, cursor_value(cursor));
        cursor_advance(cursor);
    }
    free(cursor);
}

static void do_insert(Table *table, char **tokens, int num_tokens) {
    if (num_tokens < (int)table->meta->num_columns) {
        printf("Expected %d values\n", table->meta->num_columns);
        return;
    }
    uint32_t key = (uint32_t)atoi(tokens[0]);
    char row_buffer[4096] = {0};
    if (!build_row(table->meta, tokens, num_tokens, row_buffer)) return;

    Cursor *cursor = table_find(table, key);
    void *node = get_page(table->pager, cursor->page_num);
    if (!cursor->end_of_table && *leaf_node_key(table, node, cursor->cell_num) == key) {
        printf("Error: duplicate key %d\n", key);
        free(cursor); return;
    }
    leaf_node_insert(cursor, key, row_buffer);
    free(cursor);
    printf("Inserted.\n");
}

static void do_update(Table *table, char **tokens, int num_tokens) {
    if (num_tokens < (int)table->meta->num_columns) {
        printf("Expected %d values\n", table->meta->num_columns);
        return;
    }
    uint32_t key = (uint32_t)atoi(tokens[0]);
    char row_buffer[4096] = {0};
    if (!build_row(table->meta, tokens, num_tokens, row_buffer)) return;

    Cursor *cursor = table_find(table, key);
    void *node = get_page(table->pager, cursor->page_num);
    if (cursor->end_of_table || *leaf_node_key(table, node, cursor->cell_num) != key) {
        printf("Error: key %d not found\n", key);
        free(cursor); return;
    }
    memcpy(leaf_node_value(table, node, cursor->cell_num), row_buffer, table->meta->row_size);
    free(cursor);
    printf("Updated.\n");
}

static void do_delete(Table *table, uint32_t key) {
    Cursor *cursor = table_find(table, key);
    void *node = get_page(table->pager, cursor->page_num);
    if (cursor->end_of_table || *leaf_node_key(table, node, cursor->cell_num) != key) {
        printf("Error: key %d not found\n", key);
        free(cursor); return;
    }
    leaf_node_delete(cursor);
    free(cursor);
    printf("Deleted.\n");
}

// ── Tokenize input ────────────────────────────────────────────────────────────

static int tokenize(char *buf, char **tokens, int max_tokens) {
    int count = 0;
    char *tok = strtok(buf, " \t");
    while (tok && count < max_tokens) {
        tokens[count++] = tok;
        tok = strtok(NULL, " \t");
    }
    return count;
}

// ── Main REPL ─────────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <database file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    Database *db = db_open(argv[1]);
    Table    *active = NULL;
    InputBuffer input;

    while (1) {
        if (active)
            printf("%s> ", active->meta->name);
        else
            printf("db> ");

        read_input(&input);

        // ── Meta commands ──
        if (input.buffer[0] == '.') {
            if (strcmp(input.buffer, ".exit") == 0) {
                db_close(db);
                if (active) table_close(active);
                printf("Bye!\n");
                exit(EXIT_SUCCESS);
            }
            if (strcmp(input.buffer, ".tables") == 0) {
                for (uint32_t i = 0; i < db->catalog.num_tables; i++)
                    printf("  %s\n", db->catalog.tables[i].name);
                continue;
            }
            if (strncmp(input.buffer, ".use ", 5) == 0) {
                const char *tname = input.buffer + 5;
                if (active) table_close(active);
                active = table_open(db, tname);
                if (!active) printf("Table '%s' not found.\n", tname);
                else         printf("Using table '%s'.\n", tname);
                continue;
            }
            if (strcmp(input.buffer, ".btree") == 0) {
                if (!active) { printf("No active table. Use .use <name>\n"); continue; }
                printf("Tree:\n");
                print_tree(active, active->root_page_num, 0);
                continue;
            }
            printf("Unknown command: %s\n", input.buffer);
            continue;
        }

        // ── CREATE TABLE ──
        if (strncasecmp(input.buffer, "create table ", 13) == 0) {
            TableMeta meta = {0};
            if (!parse_create_table(input.buffer, &meta)) continue;
            if (catalog_find(&db->catalog, meta.name) >= 0) {
                printf("Table '%s' already exists.\n", meta.name);
                continue;
            }
            if (db->catalog.num_tables >= MAX_TABLES) {
                printf("Max tables reached.\n"); continue;
            }
            // allocate a new root page for this table
            meta.root_page_num = get_unused_page_num(db->pager);
            void *root = get_page(db->pager, meta.root_page_num);
            initialize_leaf_node(root);
            set_node_root(root, true);

            catalog_add(&db->catalog, &meta);
            catalog_flush(db->pager, &db->catalog);
            printf("Table '%s' created.\n", meta.name);
            continue;
        }

        // ── DROP TABLE ──
        if (strncasecmp(input.buffer, "drop table ", 11) == 0) {
            const char *tname = input.buffer + 11;
            int idx = catalog_find(&db->catalog, tname);
            if (idx < 0) { printf("Table '%s' not found.\n", tname); continue; }
            if (active && strcmp(active->meta->name, tname) == 0) {
                table_close(active); active = NULL;
            }
            catalog_remove(&db->catalog, idx);
            catalog_flush(db->pager, &db->catalog);
            printf("Table '%s' dropped.\n", tname);
            continue;
        }

        // ── DML (requires active table) ──
        if (!active) {
            printf("No active table. Use .use <name> or create one.\n");
            continue;
        }

        char buf_copy[INPUT_BUFFER_SIZE];
        strncpy(buf_copy, input.buffer, INPUT_BUFFER_SIZE - 1);
        char *tokens[MAX_COLUMNS + 1];
        int   ntok = tokenize(buf_copy, tokens, MAX_COLUMNS + 1);
        if (ntok == 0) continue;

        if (strcasecmp(tokens[0], "select") == 0) {
            do_select(active);
        } else if (strcasecmp(tokens[0], "insert") == 0) {
            do_insert(active, tokens + 1, ntok - 1);
        } else if (strcasecmp(tokens[0], "update") == 0) {
            do_update(active, tokens + 1, ntok - 1);
        } else if (strcasecmp(tokens[0], "delete") == 0) {
            if (ntok < 2) { printf("Usage: delete <id>\n"); continue; }
            do_delete(active, (uint32_t)atoi(tokens[1]));
        } else {
            printf("Unknown command: %s\n", input.buffer);
        }
    }

    return 0;
}
