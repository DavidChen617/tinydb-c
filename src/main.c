#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "table.h"
#include "cursor.h"
#include "btree.h"

#define INPUT_BUFFER_SIZE 1024
#define MAX_VALUE_LEN     512

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

static int col_find(TableMeta *meta, const char *name) {
    for (uint32_t i = 0; i < meta->num_columns; i++)
        if (strcasecmp(meta->columns[i].name, name) == 0) return (int)i;
    return -1;
}

typedef struct {
    int  active;
    int  col_idx;
    char op[4];
    char value[MAX_VALUE_LEN];
} WhereClause;

static int row_matches(TableMeta *meta, void *row_data, WhereClause *wc) {
    if (!wc->active) return 1;
    Column *col   = &meta->columns[wc->col_idx];
    void   *field = (char *)row_data + col->offset;
    if (col->type == COL_INT) {
        int32_t rv; memcpy(&rv, field, sizeof(int32_t));
        int32_t cv = atoi(wc->value);
        if (strcmp(wc->op, "=")  == 0) return rv == cv;
        if (strcmp(wc->op, "!=") == 0) return rv != cv;
        if (strcmp(wc->op, ">")  == 0) return rv >  cv;
        if (strcmp(wc->op, "<")  == 0) return rv <  cv;
        if (strcmp(wc->op, ">=") == 0) return rv >= cv;
        if (strcmp(wc->op, "<=") == 0) return rv <= cv;
    } else {
        int cmp = strcmp((char *)field, wc->value);
        if (strcmp(wc->op, "=")  == 0) return cmp == 0;
        if (strcmp(wc->op, "!=") == 0) return cmp != 0;
        if (strcmp(wc->op, ">")  == 0) return cmp >  0;
        if (strcmp(wc->op, "<")  == 0) return cmp <  0;
        if (strcmp(wc->op, ">=") == 0) return cmp >= 0;
        if (strcmp(wc->op, "<=") == 0) return cmp <= 0;
    }
    return 0;
}

static void do_select(Table *table, WhereClause *wc) {
    Cursor *cursor = table_start(table);
    while (!cursor->end_of_table) {
        void *row = cursor_value(cursor);
        if (row_matches(table->meta, row, wc))
            print_row(table->meta, row);
        cursor_advance(cursor);
    }
    free(cursor);
}

static void handle_select(Database *db, Table *active, char **tokens, int ntok) {
    Table      *target = active;
    int         opened = 0;
    WhereClause wc     = {0};

    if (ntok >= 4
        && strcasecmp(tokens[1], "*")    == 0
        && strcasecmp(tokens[2], "from") == 0) {
        target = table_open(db, tokens[3]);
        if (!target) { printf("Table '%s' not found.\n", tokens[3]); return; }
        opened = 1;
        // where <col> <op> <val>
        if (ntok >= 8 && strcasecmp(tokens[4], "where") == 0) {
            int idx = col_find(target->meta, tokens[5]);
            if (idx < 0) {
                printf("Column '%s' not found.\n", tokens[5]);
                table_close(target); return;
            }
            wc.active  = 1;
            wc.col_idx = idx;
            strncpy(wc.op,    tokens[6], sizeof(wc.op)    - 1);
            strncpy(wc.value, tokens[7], sizeof(wc.value) - 1);
        }
    } else if (!target) {
        printf("No active table. Use .use <name> or 'select * from <table>'.\n");
        return;
    }

    do_select(target, &wc);
    if (opened) table_close(target);
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

    while (1) {
        char prompt[64];
        if (active)
            snprintf(prompt, sizeof(prompt), "%s> ", active->meta->name);
        else
            snprintf(prompt, sizeof(prompt), "db> ");

        char *line = readline(prompt);
        if (!line) {
            db_close(db);
            if (active) table_close(active);
            printf("Bye!\n");
            exit(EXIT_SUCCESS);
        }
        if (line[0] != '\0') add_history(line);

        char input_buf[INPUT_BUFFER_SIZE];
        strncpy(input_buf, line, INPUT_BUFFER_SIZE - 1);
        input_buf[INPUT_BUFFER_SIZE - 1] = '\0';
        free(line);

        // ── Meta commands ──
        if (input_buf[0] == '.') {
            if (strcmp(input_buf, ".exit") == 0) {
                db_close(db);
                if (active) table_close(active);
                printf("Bye!\n");
                exit(EXIT_SUCCESS);
            }
            if (strcmp(input_buf, ".tables") == 0) {
                for (uint32_t i = 0; i < db->catalog.num_tables; i++)
                    printf("  %s\n", db->catalog.tables[i].name);
                continue;
            }
            if (strncmp(input_buf, ".use ", 5) == 0) {
                const char *tname = input_buf + 5;
                if (active) table_close(active);
                active = table_open(db, tname);
                if (!active) printf("Table '%s' not found.\n", tname);
                else         printf("Using table '%s'.\n", tname);
                continue;
            }
            if (strcmp(input_buf, ".btree") == 0) {
                if (!active) { printf("No active table. Use .use <name>\n"); continue; }
                printf("Tree:\n");
                print_tree(active, active->root_page_num, 0);
                continue;
            }
            printf("Unknown command: %s\n", input_buf);
            continue;
        }

        // ── CREATE TABLE ──
        if (strncasecmp(input_buf, "create table ", 13) == 0) {
            TableMeta meta = {0};
            if (!parse_create_table(input_buf, &meta)) continue;
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
        if (strncasecmp(input_buf, "drop table ", 11) == 0) {
            const char *tname = input_buf + 11;
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

        // ── DML ──
        char buf_copy[INPUT_BUFFER_SIZE];
        strncpy(buf_copy, input_buf, INPUT_BUFFER_SIZE - 1);
        buf_copy[INPUT_BUFFER_SIZE - 1] = '\0';
        char *tokens[MAX_COLUMNS + 8];
        int   ntok = tokenize(buf_copy, tokens, MAX_COLUMNS + 8);
        if (ntok == 0) continue;

        // For insert/update/delete: optional table name as first arg
        // e.g. "insert users 1 foo"  or  "insert 1 foo" (uses active)
        if (strcasecmp(tokens[0], "select") == 0) {
            handle_select(db, active, tokens, ntok);
        } else if (strcasecmp(tokens[0], "insert") == 0) {
            int opened = 0;
            char **vals = tokens + 1; int nvals = ntok - 1;
            Table *t = active;
            if (nvals > 0 && catalog_find(&db->catalog, vals[0]) >= 0) {
                t = table_open(db, vals[0]); opened = 1; vals++; nvals--;
            }
            if (!t) printf("No active table.\n");
            else    do_insert(t, vals, nvals);
            if (opened && t) table_close(t);
        } else if (strcasecmp(tokens[0], "update") == 0) {
            int opened = 0;
            char **vals = tokens + 1; int nvals = ntok - 1;
            Table *t = active;
            if (nvals > 0 && catalog_find(&db->catalog, vals[0]) >= 0) {
                t = table_open(db, vals[0]); opened = 1; vals++; nvals--;
            }
            if (!t) printf("No active table.\n");
            else    do_update(t, vals, nvals);
            if (opened && t) table_close(t);
        } else if (strcasecmp(tokens[0], "delete") == 0) {
            int opened = 0;
            char **args = tokens + 1; int nargs = ntok - 1;
            Table *t = active;
            if (nargs > 1 && catalog_find(&db->catalog, args[0]) >= 0) {
                t = table_open(db, args[0]); opened = 1; args++; nargs--;
            }
            if (!t)        printf("No active table.\n");
            else if (!nargs) printf("Usage: delete [<table>] <id>\n");
            else           do_delete(t, (uint32_t)atoi(args[0]));
            if (opened && t) table_close(t);
        } else {
            printf("Unknown command: %s\n", input_buf);
        }
    }

    return 0;
}
