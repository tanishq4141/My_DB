# MiniDB — A Minimal Relational Database Engine

MiniDB is an educational relational database engine built from scratch in C++17. It implements core database internals including a SQL parser, storage engine with buffer pool, B+ tree indexing, a cost-based query optimizer, ACID transactions via strict two-phase locking, and Write-Ahead Logging for crash recovery.

## Architecture

```
┌──────────────────────────────────────────────┐
│                 SQL REPL                     │
├──────────────────────────────────────────────┤
│     Lexer  →  Parser  →  AST (Statement)    │
├──────────────────────────────────────────────┤
│         Cost-Based Query Optimizer           │
│  (selectivity estimation, index vs scan)     │
├──────────────────────────────────────────────┤
│         Executor (Iterator Model)            │
│  SeqScan │ IndexScan │ Join │ Filter │ Proj  │
├──────────────────────────────────────────────┤
│    Transaction Manager  │  Lock Manager      │
│    (Strict 2PL, deadlock detection)          │
├──────────────┬──────────┬────────────────────┤
│  PageManager │ B+Tree   │  WAL LogManager    │
│  (Heap File) │ Index    │  (Crash Recovery)  │
├──────────────┴──────────┴────────────────────┤
│         Buffer Pool (LRU Eviction)           │
├──────────────────────────────────────────────┤
│         Disk Manager (Raw File I/O)          │
└──────────────────────────────────────────────┘
```

## Project Structure

```
My_DB/
├── Makefile                    # Build configuration
├── README.md                   # This file
├── src/
│   ├── main.cpp               # MiniDB engine + REPL
│   ├── common/                # Shared types and utilities
│   │   ├── config.h           # Global constants (page size, pool size, etc.)
│   │   ├── rid.h              # Record Identifier (pageId, slotNum)
│   │   ├── value.h            # Value type (INT or TEXT variant)
│   │   ├── column.h           # Column definition (name + type)
│   │   ├── schema.h           # Table schema (list of columns)
│   │   ├── tuple.h            # Tuple (ordered list of values)
│   │   └── row.h              # Row (tuple + row ID)
│   ├── parser/                # SQL lexer and parser
│   │   ├── lexer.h / .cpp     # Tokenizer
│   │   └── parser.h / .cpp    # Recursive-descent parser
│   ├── storage/               # Storage engine
│   │   ├── DiskManager.h/.cpp # Raw page-level file I/O
│   │   ├── BufferPool.h/.cpp  # LRU page cache
│   │   ├── PageManager.h/.cpp # Slotted-page heap file manager
│   │   └── Catalog.h          # Table metadata catalog
│   ├── index/                 # Indexing
│   │   └── BPlusTree.h/.cpp   # B+ tree index implementation
│   ├── query/                 # Query execution
│   │   └── Executor.h/.cpp    # Iterator-model executors
│   ├── optimizer/             # Query optimization
│   │   └── Optimizer.h/.cpp   # Cost-based optimizer
│   ├── transaction/           # Concurrency control
│   │   ├── LockManager.h/.cpp # Strict 2PL lock manager
│   │   └── Transaction.h/.cpp # Transaction lifecycle
│   └── recovery/              # Durability
│       ├── LogManager.h/.cpp  # Write-Ahead Logging
│       └── Recovery.h/.cpp    # Crash recovery (redo/undo)
├── benchmarks/                # Performance benchmarks
│   └── benchmark.sh           # Automated benchmark script
└── docs/                      # Additional documentation
    └── design.md              # Detailed design document
```

## Building

Requirements: C++17 compiler (g++ or clang++), Make

```bash
make
```

This produces the `minidb` executable.

## Running

```bash
./minidb
```

### Example Session

```sql
minidb> CREATE TABLE students (id INT, name TEXT, grade INT);
Table 'students' created with 3 columns.

minidb> INSERT INTO students VALUES (1, 'Alice', 95);
Inserted 1 row into 'students'.

minidb> INSERT INTO students VALUES (2, 'Bob', 87);
Inserted 1 row into 'students'.

minidb> SELECT * FROM students;
+---------------+---------------+---------------+
|id             |name           |grade          |
+---------------+---------------+---------------+
|1              |Alice          |95             |
|2              |Bob            |87             |
+---------------+---------------+---------------+
2 row(s) returned.

minidb> SELECT name FROM students WHERE grade = 95;
+---------------+
|name           |
+---------------+
|Alice          |
+---------------+
1 row(s) returned.

minidb> DELETE FROM students WHERE id = 2;
Deleted 1 row(s) from 'students'.

minidb> .exit
Goodbye!
```

## Components

### Storage Engine
- **DiskManager**: Manages raw file I/O with fixed-size pages (4KB)
- **BufferPool**: LRU page cache with pin/unpin semantics
- **PageManager**: Slotted-page heap file manager for tuple storage
- **Catalog**: In-memory metadata about tables, schemas, and statistics

### B+ Tree Index
- Balanced tree with configurable order (default 128)
- Supports search, insert, delete, and range scan
- Automatic split/merge on overflow/underflow
- Used as primary key index for tables

### SQL Parser
- Hand-written recursive-descent parser
- Supports: CREATE TABLE, INSERT, SELECT, DELETE
- WHERE clause with equality predicates

### Query Optimizer
- Cost-based optimization using selectivity estimation
- Chooses between sequential scan and index scan
- Considers table size and predicate selectivity

### Executor
- Iterator model (init/next interface)
- Operators: SeqScan, IndexScan, Insert, Delete, NestedLoopJoin, Filter, Projection

### Transaction Manager
- Strict two-phase locking (2PL)
- Shared and exclusive lock modes
- Deadlock detection via wait-for graph

### WAL & Recovery
- Write-Ahead Logging with log record serialization
- ARIES-style crash recovery (redo committed, undo uncommitted)
- Checkpoint support

## Dependencies

- C++17 standard library
- POSIX filesystem (for directory creation)
- pthreads (for mutex/locking)

No external libraries required.
