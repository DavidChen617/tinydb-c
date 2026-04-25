# SimpleDB

[English](README.md) | [繁體中文](README-zh-TW.md)

以 C 語言從零實作的關聯式資料庫引擎，具備 B-Tree 索引、多表支援、動態 Schema 與類 SQL 的 REPL 介面。

## 功能特色

- **多表支援** — 在單一 `.db` 檔案中建立並管理多張資料表
- **自訂 Schema** — 建表時自行定義欄位，支援 `INT` 與 `TEXT(n)` 型別
- **完整 CRUD** — `INSERT`、`SELECT`、`UPDATE`、`DELETE`
- **WHERE 條件** — 支援 `=`、`!=`、`>`、`<`、`>=`、`<=` 過濾資料
- **B-Tree 索引** — Leaf / Internal Node 分裂，以及 DELETE 後的再平衡（借用、合併、Root Collapse）
- **磁碟持久化** — 以 4 KB Page 為單位的 Buffer Pool，資料存於單一檔案
- **System Catalog** — 所有表格的 Metadata 儲存於 Page 0，重新開啟後自動還原
- **readline 支援** — REPL 中可使用方向鍵與歷史指令

## 架構

```
REPL (main.c)
  └── 指令解析
        ├── System Catalog (catalog.c)   ← 表格 Metadata 儲存於 Page 0
        └── Cursor (cursor.c)
              └── B-Tree (btree.c)
                    └── Pager / Buffer Pool (pager.c)
                          └── 資料庫檔案 (.db)
```

| 檔案 | 職責 |
|------|------|
| `schema.h` | `Column` / `TableMeta` 定義 |
| `catalog.h/c` | Catalog 的讀寫；表格的新增／查詢／刪除 |
| `table.h/c` | `Database`（檔案層）與 `Table`（使用中的表）；B-Tree 執行期常數計算 |
| `btree.h/c` | B-Tree 節點、分裂、再平衡、搜尋 |
| `cursor.h/c` | 循序與 Key 查詢的 Cursor |
| `pager.h/c` | Page 快取；檔案讀寫 |
| `main.c` | REPL、Schema 與 DML 解析 |

## 建置與執行

**相依套件：** `readline`（macOS：Xcode CLT 已內建；Linux：`sudo apt install libreadline-dev`）

```bash
make
./simpledb <資料庫檔案>
```

## 使用方式

### 資料表管理

```
db> create table users (id INT, name TEXT(50), age INT)
db> create table products (id INT, title TEXT(80), price INT)
db> .tables
db> drop table products
```

第一個欄位必須是 `INT`，作為 Primary Key。

### 切換使用中的表格

```
db> .use users
users>
```

### INSERT

```sql
-- 已用 .use 切換表格
insert 1 alice 30

-- 直接指定表格名稱（無需 .use）
insert users 1 alice 30
```

### SELECT

```sql
-- 全部資料
select * from users

-- 帶條件
select * from users where age = 30
select * from users where name = alice
select * from users where age >= 25
select * from users where id != 2
```

支援運算子：`=` `!=` `>` `<` `>=` `<=`

### UPDATE

```sql
update users 1 alice 31
-- 或已 .use 的情況下：
update 1 alice 31
```

需提供所有欄位的值（含 id）。

### DELETE

```sql
delete users 1
-- 或已 .use 的情況下：
delete 1
```

### 除錯指令

```
.btree      -- 印出目前表格的 B-Tree 結構
.exit       -- 寫入磁碟並離開
```

## 使用範例

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

## 儲存格式

| Page | 內容 |
|------|------|
| 0 | System Catalog（`num_tables` + `TableMeta[]`） |
| 1…N | 各表格的 B-Tree 節點 |

每個 B-Tree Leaf Cell：`uint32_t key` + 資料列位元組（長度由 Schema 決定）。

## 技術棧

| 元件 | 技術 |
|------|------|
| 語言 | C (C11) |
| 建置工具 | GNU Make |
| 儲存層 | Page-based 檔案 I/O（每頁 4 KB） |
| 索引結構 | B-Tree（Leaf Node + Internal Node） |
| 輸入處理 | GNU readline |
| 平台 | macOS / Linux |
