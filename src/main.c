#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "table.h"
#include "cursor.h"
#include "btree.h"

#define INPUT_BUFFER_SIZE 1024 

typedef struct 
{
    char buffer[INPUT_BUFFER_SIZE];
} InputBuffer;

typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT
} StatementType;

typedef struct
{
    StatementType type;
    Row row_to_insert;
} Statement;

void print_prompt(){
    printf("db > ");
}

void read_input(InputBuffer *input){
    if(fgets(input->buffer, INPUT_BUFFER_SIZE, stdin) == NULL){
        printf("Error reading input\n");
        exit(EXIT_FAILURE);
    }

    int len = strlen(input->buffer);
    if(len > 0 && input->buffer[len - 1] == '\n'){
        input->buffer[len-1] = '\0';
    }
}

// 解析 SQL 指令
int prepare_statement(InputBuffer *input, Statement *statement){
    if(strncmp(input->buffer, "insert", 6) == 0){
        statement->type = STATEMENT_INSERT;
        int args = sscanf(
                input->buffer, "insert %d %32s %255s",
                &statement->row_to_insert.id,
                statement->row_to_insert.username,
                statement->row_to_insert.email);
            
        if(args < 3){
            printf("Syntax error. Usage: insert <id> <username> <email>\n");
            return 0;
        }
        return 1;
    }
    if(strncmp(input->buffer, "select", 6) == 0){
        statement->type = STATEMENT_SELECT;
        return 1;
    }

    return 0;
}

// 執行 SQL 指令
void execute_statement(Statement *statement, Table *table){
    Cursor *cursor;
    switch(statement->type){
        case STATEMENT_INSERT:
            uint32_t key = statement->row_to_insert.id;
            cursor = table_find(table, key);
            void *node = get_page(table->pager, cursor->page_num);

            if(!cursor->end_of_table && *leaf_node_key(node, cursor->cell_num) == key){
                printf("Error: duplicate key %d\n", key);
                free(cursor);
                return;
            }

            leaf_node_insert(cursor, key, &statement->row_to_insert);
            free(cursor); 
            printf("Inseted.\n");
            break;

        case STATEMENT_SELECT:
            cursor = table_start(table);
            Row row;

            while (!cursor->end_of_table)
            {
                deserialize_row(cursor_value(cursor), &row);
                print_row(&row);
                cursor_advance(cursor);
            }
    
            free(cursor);
            break;
    }
}

int main(int argc, char *argv[]){
    if(argc < 2){
        printf("Usage: %s <database file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    Table *table = db_open(argv[1]);
    InputBuffer input;

    while(1){
        print_prompt();
        read_input(&input);

        if (strcmp(input.buffer, ".btree") == 0) {
            printf("Tree:\n");                    
            print_tree(table->pager, table->root_page_num, 0);
            continue;            
        }

        if(input.buffer[0] == '.'){
            if(strcmp(input.buffer, ".exit") == 0){
                printf("Bye!\n");
                db_close(table);
                exit(EXIT_SUCCESS);
            }
            else{
                printf("Unrecognized command: %s\n", input.buffer);
            }
            continue;
        }

        Statement statement;
        if(!prepare_statement(&input, &statement)){
            printf("Unrecognized keyword: %s\n", input.buffer);
            continue;
        }

        execute_statement(&statement, table);
    }

    return 0;
}
