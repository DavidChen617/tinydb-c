# SimpleDB

以 C 語言從零實作的簡易關聯式資料庫引擎，目的是深入理解資料庫的內部運作原理。

## 專案說明

SimpleDB 是一個單表、磁碟持久化的資料庫引擎，提供 REPL 互動介面。支援基本的 SQL 指令，並使用 B-Tree 資料結構儲存資料。

**功能：**
- 互動式 REPL（`db >` 提示符）
- `INSERT` 與 `SELECT` 指令
- B-Tree 索引儲存，支援 Leaf Node 與 Internal Node 分裂
- 透過 Page-based Buffer Pool（Pager）實現磁碟持久化
- Cursor 抽象層，支援循序與 Key 查詢
- 重複 Key 偵測

**架構：**

```
REPL (main.c)
  └── 指令解析
        └── Cursor (cursor.c)
              └── B-Tree (btree.c)
                    └── Pager / Buffer Pool (pager.c)
                          └── 資料庫檔案 (.db)
```

## 技術棧

| 元件 | 技術 |
|------|------|
| 語言 | C (C11) |
| 建置工具 | GNU Make |
| 儲存層 | Page-based 檔案 I/O（每頁 4KB） |
| 索引結構 | B-Tree（Leaf Node + Internal Node） |
| 平台 | Linux |

## 建置與執行

```bash
make
./simpledb <資料庫檔案>
```

**範例：**

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

**Meta 指令：**

| 指令 | 說明 |
|------|------|
| `.exit` | 儲存並離開 |
| `.btree` | 印出 B-Tree 結構 |

## 資料列格式

每筆資料使用固定長度的二進位格式：

| 欄位 | 型別 | 大小 |
|------|------|------|
| id | uint32 | 4 bytes |
| username | char[33] | 33 bytes |
| email | char[256] | 256 bytes |

## 參考來源
> **"Let's Build a Simple Database"** by cstack
> https://cstack.github.io/db_tutorial/
