#pragma once
#include "../common/rid.h"
#include "../common/schema.h"
#include "../common/tuple.h"
#include "../common/value.h"
#include "../index/BPlusTree.h"
#include "../storage/Catalog.h"
#include "../storage/PageManager.h"
#include <functional>
#include <string>
#include <vector>

// Forward declaration
class ExecutionContext;

// ============================================================
// Base Executor class — iterator model: init() + next()
// ============================================================
class Executor {
public:
  virtual ~Executor() = default;

  // Initialize the executor. Must be called before next().
  virtual void init(ExecutionContext *ctx) = 0;

  // Fetch the next tuple. Returns false when no more tuples.
  virtual bool next(Tuple &tuple) = 0;

  // Get the output schema of this executor
  virtual const Schema &outputSchema() const = 0;
};

// ============================================================
// Execution context: shared state for all executors in a plan
// ============================================================
class ExecutionContext {
public:
  ExecutionContext(Catalog *catalog, PageManager *pageMgr,
                  DiskManager *diskMgr, BufferPool *bufferPool)
      : catalog_(catalog), pageMgr_(pageMgr), diskMgr_(diskMgr),
        bufferPool_(bufferPool) {}

  Catalog *catalog() { return catalog_; }
  PageManager *pageManager() { return pageMgr_; }
  DiskManager *diskManager() { return diskMgr_; }
  BufferPool *bufferPool() { return bufferPool_; }

  // B+ Tree indexes keyed by table name
  void setIndex(const std::string &tableName, BPlusTree *tree) {
    indexes_[tableName] = tree;
  }
  BPlusTree *getIndex(const std::string &tableName) {
    auto it = indexes_.find(tableName);
    return (it != indexes_.end()) ? it->second : nullptr;
  }

private:
  Catalog *catalog_;
  PageManager *pageMgr_;
  DiskManager *diskMgr_;
  BufferPool *bufferPool_;
  std::unordered_map<std::string, BPlusTree *> indexes_;
};

// ============================================================
// SeqScan Executor — full table scan with optional predicate
// ============================================================
class SeqScanExecutor : public Executor {
public:
  SeqScanExecutor(const std::string &tableName,
                  std::function<bool(const Tuple &)> predicate = nullptr);

  void init(ExecutionContext *ctx) override;
  bool next(Tuple &tuple) override;
  const Schema &outputSchema() const override { return schema_; }

private:
  std::string tableName_;
  std::function<bool(const Tuple &)> predicate_;
  Schema schema_;
  int fileId_;

  // Scan state
  std::vector<Tuple> results_;
  size_t currentIdx_;

  // Deserialize raw bytes into a Tuple using the schema
  Tuple deserialize(const char *data, uint16_t len, const Schema &schema);
};

// ============================================================
// IndexScan Executor — uses B+Tree to satisfy equality predicates
// ============================================================
class IndexScanExecutor : public Executor {
public:
  IndexScanExecutor(const std::string &tableName, int searchKey);

  void init(ExecutionContext *ctx) override;
  bool next(Tuple &tuple) override;
  const Schema &outputSchema() const override { return schema_; }

private:
  std::string tableName_;
  int searchKey_;
  Schema schema_;
  int fileId_;
  bool done_;
  RID resultRid_;

  Tuple deserialize(const char *data, uint16_t len, const Schema &schema);
};

// ============================================================
// Insert Executor — inserts a tuple into a table
// ============================================================
class InsertExecutor : public Executor {
public:
  InsertExecutor(const std::string &tableName,
                 const std::vector<Value> &values);

  void init(ExecutionContext *ctx) override;
  bool next(Tuple &tuple) override;
  const Schema &outputSchema() const override { return schema_; }

private:
  std::string tableName_;
  std::vector<Value> values_;
  Schema schema_;
  bool done_;

  // Serialize a tuple to raw bytes
  std::vector<char> serialize(const Tuple &tuple, const Schema &schema);
};

// ============================================================
// Delete Executor — deletes tuples from a table (using child scan)
// ============================================================
class DeleteExecutor : public Executor {
public:
  DeleteExecutor(const std::string &tableName,
                 std::function<bool(const Tuple &)> predicate = nullptr);

  void init(ExecutionContext *ctx) override;
  bool next(Tuple &tuple) override;
  const Schema &outputSchema() const override { return schema_; }

private:
  std::string tableName_;
  std::function<bool(const Tuple &)> predicate_;
  Schema schema_;
  int fileId_;
  bool done_;
  int deletedCount_;
};

// ============================================================
// Nested Loop Join Executor
// ============================================================
class NestedLoopJoinExecutor : public Executor {
public:
  NestedLoopJoinExecutor(Executor *left, Executor *right,
                         const std::string &leftCol,
                         const std::string &rightCol);

  void init(ExecutionContext *ctx) override;
  bool next(Tuple &tuple) override;
  const Schema &outputSchema() const override { return outputSchema_; }

private:
  Executor *left_;
  Executor *right_;
  std::string leftCol_;
  std::string rightCol_;
  Schema outputSchema_;

  // Pre-materialized results for nested loop
  std::vector<Tuple> leftTuples_;
  std::vector<Tuple> rightTuples_;
  std::vector<Tuple> joinResults_;
  size_t currentIdx_;
};

// ============================================================
// Filter Executor — applies a predicate to filter tuples
// ============================================================
class FilterExecutor : public Executor {
public:
  FilterExecutor(Executor *child,
                 std::function<bool(const Tuple &)> predicate);

  void init(ExecutionContext *ctx) override;
  bool next(Tuple &tuple) override;
  const Schema &outputSchema() const override {
    return child_->outputSchema();
  }

private:
  Executor *child_;
  std::function<bool(const Tuple &)> predicate_;
};

// ============================================================
// Projection Executor — selects specific columns
// ============================================================
class ProjectionExecutor : public Executor {
public:
  ProjectionExecutor(Executor *child,
                     const std::vector<std::string> &columns);

  void init(ExecutionContext *ctx) override;
  bool next(Tuple &tuple) override;
  const Schema &outputSchema() const override { return outputSchema_; }

private:
  Executor *child_;
  std::vector<std::string> columns_;
  Schema outputSchema_;
  std::vector<int> columnIndices_;
};
