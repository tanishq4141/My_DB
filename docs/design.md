# MiniDB Design Document

## Overview

MiniDB is a layered relational database engine designed for educational purposes. It implements the fundamental components found in production database systems, organized into clearly separated modules.

## Layered Architecture

The system follows a strict layered architecture where each layer only communicates with its immediate neighbors:

### Layer 1: SQL Interface (Parser)
- **Lexer**: Tokenizes SQL text into a stream of typed tokens (keywords, identifiers, literals, symbols)
- **Parser**: Recursive-descent parser that consumes tokens and produces a `Statement` AST
- Supported SQL: `CREATE TABLE`, `INSERT INTO ... VALUES`, `SELECT ... FROM ... WHERE`, `DELETE FROM ... WHERE`

### Layer 2: Query Optimizer
- Transforms the AST into a physical query plan (tree of `PlanNode`)
- Cost-based decisions:
  - **Scan selection**: Chooses between sequential scan and index scan based on selectivity and table size
  - **Selectivity estimation**: Uses uniform distribution heuristic (1/N for equality, configurable)
  - **Cost model**: I/O cost proportional to pages scanned

### Layer 3: Executor (Iterator Model)
- Each operator implements `init()` / `next()` interface
- Operators pull tuples from children (Volcano model)
- Available operators:
  - `SeqScanExecutor`: Full table scan with optional predicate pushdown
  - `IndexScanExecutor`: B+ tree lookup for equality predicates
  - `InsertExecutor`: Serializes and stores new tuples
  - `DeleteExecutor`: Removes matching tuples
  - `NestedLoopJoinExecutor`: Cartesian product with equi-join filter
  - `FilterExecutor`: Applies arbitrary predicate
  - `ProjectionExecutor`: Column selection

### Layer 4: Storage Engine
- **DiskManager**: Raw page I/O to table data files
- **BufferPool**: Fixed-size page cache with LRU eviction
  - Pin/unpin semantics for concurrent access
  - Dirty page tracking and writeback
- **PageManager**: Slotted-page heap file manager
  - Variable-length tuple storage
  - Slot directory for O(1) tuple access within a page
- **Catalog**: In-memory metadata (schema, statistics, file mappings)

### Layer 5: Index
- **B+ Tree**: Balanced tree index on integer keys
  - Internal nodes with sorted keys and child pointers
  - Leaf nodes with sorted (key, RID) entries and next-leaf pointers
  - Split on overflow, merge/redistribute on underflow
  - Range scan via leaf-level linked list traversal

### Layer 6: Transaction & Concurrency
- **LockManager**: Strict two-phase locking
  - Shared locks for reads, exclusive locks for writes
  - Wait-for graph for deadlock detection (DFS cycle detection)
- **TransactionManager**: Begin/commit/abort lifecycle
  - Locks released only at commit/abort (shrinking phase)

### Layer 7: Recovery
- **LogManager**: Write-Ahead Logging
  - Log records serialized with (LSN, type, txnId, table, RID, old/new data)
  - Forced flush on commit for durability
- **RecoveryManager**: Simplified ARIES
  - Analysis: determine committed vs. active transactions
  - Redo: replay committed operations
  - Undo: reverse uncommitted operations

## Data Format

### Tuple Serialization
Each value is serialized as:
- `0x01` + 4-byte int (for INT type)
- `0x02` + 2-byte length + string bytes (for TEXT type)

### Slotted Page Layout
```
+----------------------------------+
| PageHeader (pageId, numSlots,    |
|   freeSpacePtr, slotDir[],       |
|   slotLengths[])                 |
+----------------------------------+
| Tuple 0 data                     |
| Tuple 1 data                     |
| ...                              |
| (free space)                     |
+----------------------------------+
```

## Extension Track: B+ Tree Index (Track A - Performance)

The B+ tree index provides logarithmic-time point lookups compared to linear-time sequential scans. The optimizer automatically selects index scan when:
1. A B+ tree exists on the queried column
2. The predicate is an equality check on the indexed column
3. The table has more than 50 rows (below this, sequential scan is faster due to lower overhead)

### Performance Comparison
- Sequential scan: O(N) pages read
- Index scan: O(log N) pages traversed + 1 page read for the tuple
- For a table with 10,000 rows (~1,000 pages), index scan is ~100x faster for point queries
