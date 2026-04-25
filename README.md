# SimpleDB

[English](README.md) | [繁體中文](README-zh-TW.md)

A relational database engine implemented in C from scratch, featuring a B-Tree index, multi-table support, dynamic schemas, and a SQL-like REPL.

## Features

- **Multi-table** — create and manage multiple tables in a single `.db` file
- **Custom schemas** — define columns with `INT` or `TEXT(n)` types at table creation time
- **Full CRUD** — `INSERT`, `SELECT`, `UPDATE`, `DELETE`
- **WHERE clause** — filter rows with `=`, `!=`, `>`, `<`, `>=`, `<=`
- **B-Tree index** — leaf/internal node splits and rebalancing (borrow + merge + root collapse) on delete
- **Persistent storage** — page-based buffer pool (4 KB pages) backed by a single file
- **System catalog** — table metadata stored on page 0, survives restarts
- **readline** — arrow keys, command history in the REPL

## Architecture

```
REPL (main.c)
  └── Statement Parser
        ├── System Catalog (catalog.c)   ← table metadata on page 0
        └── Cursor (cursor.c)
              └── B-Tree (btree.c)
                    └── Pager / Buffer Pool (pager.c)
                          └── Database File (.db)
```

| File | Role |
|------|------|
| `schema.h` | `Column` / `TableMeta` definitions |
| `catalog.h/c` | Load/flush catalog; find/add/remove tables |
| `table.h/c` | `Database` (file) and `Table` (active handle); runtime B-Tree constants |
| `btree.h/c` | B-Tree nodes, splits, rebalancing, search |
| `cursor.h/c` | Sequential and key-based cursor |
| `pager.h/c` | Page cache; file read/write |
| `main.c` | REPL, schema/DML parsing |

## Build & Run

**Dependencies:** `readline` (macOS: ships with Xcode CLT; Linux: `sudo apt install libreadline-dev`)

```bash
make
./simpledb <database_file>
```

## Usage

### Table management

```
db> create table users (id INT, name TEXT(50), age INT)
db> create table products (id INT, title TEXT(80), price INT)
db> .tables
db> drop table products
```

The first column must be `INT` and serves as the primary key.

### Switch active table

```
db> .use users
users>
```

### INSERT

```sql
-- with active table
insert 1 alice 30

-- without .use (specify table name)
insert users 1 alice 30
```

### SELECT

```sql
-- all rows
select * from users

-- with filter
select * from users where age = 30
select * from users where name = alice
select * from users where age >= 25
select * from users where id != 2
```

Supported operators: `=` `!=` `>` `<` `>=` `<=`

### UPDATE

```sql
update users 1 alice 31
-- or with active table:
update 1 alice 31
```

Provide all column values (including id).

### DELETE

```sql
delete users 1
-- or with active table:
delete 1
```

### Debug

```
.btree      -- print B-Tree structure of the active table
.exit       -- flush and exit
```

## Example Session

```
$ ./simpledb school.db
db> create table students (id INT, name TEXT(50), grade INT)
Table 'students' created.
db> insert students 1 alice 90
Inserted.
db> insert students 2 bob 75
Inserted.
db> insert students 3 charlie 90
Inserted.
db> select * from students where grade = 90
(1, alice, 90)
(3, charlie, 90)
db> update students 2 bob 80
Updated.
db> delete students 3
Deleted.
db> select * from students
(1, alice, 90)
(2, bob, 80)
db> .btree
Tree:
- leaf (size 2)
  - 1
  - 2
db> .exit
Bye!
```

## Storage Layout

| Page | Content |
|------|---------|
| 0 | System catalog (`num_tables` + `TableMeta[]`) |
| 1…N | B-Tree nodes for each table |

Each B-Tree leaf cell: `uint32_t key` + row bytes (layout determined by schema).

## Tech Stack

| Component | Technology |
|-----------|------------|
| Language | C (C11) |
| Build | GNU Make |
| Storage | Page-based file I/O (4 KB pages) |
| Index | B-Tree (leaf + internal nodes) |
| Input | GNU readline |
| Platform | macOS / Linux |
