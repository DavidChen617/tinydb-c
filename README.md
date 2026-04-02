# SimpleDB

A simple but functional relational database engine implemented in C, built from scratch to understand how databases work internally.

## What This Project Does

SimpleDB is a single-table, disk-backed database engine with a REPL interface. It supports basic SQL-like commands and stores data persistently using a B-Tree data structure.

**Features:**
- Interactive REPL (`db >` prompt)
- `INSERT` and `SELECT` commands
- B-Tree indexed storage with leaf and internal node splits
- Persistent storage to disk via a page-based buffer pool (Pager)
- Cursor abstraction for sequential and keyed access
- Duplicate key detection

**Architecture:**

```
REPL (main.c)
  └── Statement Parser
        └── Cursor (cursor.c)
              └── B-Tree (btree.c)
                    └── Pager / Buffer Pool (pager.c)
                          └── Database File (.db)
```

## Tech Stack

| Component | Technology |
|-----------|------------|
| Language | C (C11) |
| Build System | GNU Make |
| Storage | Page-based file I/O (4KB pages) |
| Index Structure | B-Tree (leaf + internal nodes) |
| Platform | Linux |

## Build & Run

```bash
make
./simpledb <database_file>
```

**Example:**

```bash
./simpledb data/mydb.db
db > insert 1 alice alice@example.com
Inseted.
db > insert 2 bob bob@example.com
Inseted.
db > select
(1, alice, alice@example.com)
(2, bob, bob@example.com)
db > .btree
Tree:
- leaf (size 2)
  - 1
  - 2
db > .exit
Bye!
```

**Meta commands:**

| Command | Description |
|---------|-------------|
| `.exit` | Save and exit |
| `.btree` | Print B-Tree structure |

## Row Schema

Each row has a fixed binary layout:

| Field | Type | Size |
|-------|------|------|
| id | uint32 | 4 bytes |
| username | char[33] | 33 bytes |
| email | char[256] | 256 bytes |

## Reference

> **"Let's Build a Simple Database"** by cstack
> https://cstack.github.io/db_tutorial/
