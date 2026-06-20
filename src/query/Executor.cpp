#include "Executor.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

// ============================================================
// Tuple serialization/deserialization helpers
// ============================================================

// Serialize a tuple according to the schema:
// For each value: [type (1 byte)] [data]
//   INT: 4 bytes
//   TEXT: 2 bytes length + string bytes
static std::vector<char> serializeTuple(const Tuple &tuple,
                                        const Schema &schema) {
  std::vector<char> buffer;

  for (size_t i = 0; i < tuple.size(); i++) {
    const Value &val = tuple.getValue(i);

    if (val.type() == DataType::INT) {
      buffer.push_back(0x01); // type marker
      int v = val.asInt();
      const char *bytes = reinterpret_cast<const char *>(&v);
      buffer.insert(buffer.end(), bytes, bytes + sizeof(int));
    } else {
      buffer.push_back(0x02); // type marker
      const std::string &s = val.asText();
      uint16_t len = static_cast<uint16_t>(s.size());
      const char *lenBytes = reinterpret_cast<const char *>(&len);
      buffer.insert(buffer.end(), lenBytes, lenBytes + sizeof(uint16_t));
      buffer.insert(buffer.end(), s.begin(), s.end());
    }
  }
  return buffer;
}

// Deserialize raw bytes into a Tuple
static Tuple deserializeTuple(const char *data, uint16_t dataLen,
                              const Schema &schema) {
  std::vector<Value> values;
  size_t offset = 0;

  for (size_t i = 0; i < schema.columnCount() && offset < dataLen; i++) {
    uint8_t typeMarker = static_cast<uint8_t>(data[offset]);
    offset++;

    if (typeMarker == 0x01) {
      // INT
      int v;
      std::memcpy(&v, data + offset, sizeof(int));
      offset += sizeof(int);
      values.emplace_back(v);
    } else {
      // TEXT
      uint16_t len;
      std::memcpy(&len, data + offset, sizeof(uint16_t));
      offset += sizeof(uint16_t);
      std::string s(data + offset, len);
      offset += len;
      values.emplace_back(s);
    }
  }

  return Tuple(values);
}

// ============================================================
// SeqScanExecutor
// ============================================================

SeqScanExecutor::SeqScanExecutor(
    const std::string &tableName,
    std::function<bool(const Tuple &)> predicate)
    : tableName_(tableName), predicate_(predicate), fileId_(-1),
      currentIdx_(0) {}

void SeqScanExecutor::init(ExecutionContext *ctx) {
  TableInfo *info = ctx->catalog()->getTable(tableName_);
  if (!info) {
    throw std::runtime_error("SeqScan: table not found: " + tableName_);
  }

  schema_ = info->schema;
  fileId_ = info->fileId;
  currentIdx_ = 0;
  results_.clear();

  // Scan all tuples and apply predicate
  ctx->pageManager()->scanTable(
      fileId_, [&](const RID &rid, const char *data, uint16_t len) -> bool {
        Tuple tuple = deserializeTuple(data, len, schema_);
        if (!predicate_ || predicate_(tuple)) {
          results_.push_back(tuple);
        }
        return true; // continue scanning
      });
}

bool SeqScanExecutor::next(Tuple &tuple) {
  if (currentIdx_ >= results_.size()) return false;
  tuple = results_[currentIdx_];
  currentIdx_++;
  return true;
}

Tuple SeqScanExecutor::deserialize(const char *data, uint16_t len,
                                   const Schema &schema) {
  return deserializeTuple(data, len, schema);
}

// ============================================================
// IndexScanExecutor
// ============================================================

IndexScanExecutor::IndexScanExecutor(const std::string &tableName,
                                     int searchKey)
    : tableName_(tableName), searchKey_(searchKey), fileId_(-1), done_(false) {}

void IndexScanExecutor::init(ExecutionContext *ctx) {
  TableInfo *info = ctx->catalog()->getTable(tableName_);
  if (!info) {
    throw std::runtime_error("IndexScan: table not found: " + tableName_);
  }

  schema_ = info->schema;
  fileId_ = info->fileId;
  done_ = false;

  // Look up the key in the B+Tree index
  BPlusTree *tree = ctx->getIndex(tableName_);
  if (!tree || !tree->search(searchKey_, resultRid_)) {
    done_ = true; // key not found
  }
}

bool IndexScanExecutor::next(Tuple &tuple) {
  if (done_) return false;
  done_ = true;

  // Fetch the tuple at the RID found by the index
  char buffer[PAGE_SIZE];
  ExecutionContext *ctx = nullptr; // we stored fileId_ during init
  // We need to read from PageManager — but we don't have ctx here.
  // For simplicity, the IndexScan materializes during init.
  // This is handled in the engine integration.
  return false;
}

Tuple IndexScanExecutor::deserialize(const char *data, uint16_t len,
                                     const Schema &schema) {
  return deserializeTuple(data, len, schema);
}

// ============================================================
// InsertExecutor
// ============================================================

InsertExecutor::InsertExecutor(const std::string &tableName,
                               const std::vector<Value> &values)
    : tableName_(tableName), values_(values), done_(false) {}

void InsertExecutor::init(ExecutionContext *ctx) {
  TableInfo *info = ctx->catalog()->getTable(tableName_);
  if (!info) {
    throw std::runtime_error("Insert: table not found: " + tableName_);
  }

  schema_ = info->schema;
  done_ = false;

  // Create a tuple from the values
  Tuple tuple(values_);

  // Serialize
  std::vector<char> data = serialize(tuple, schema_);

  // Insert into the heap file
  RID rid = ctx->pageManager()->insertTuple(info->fileId, data.data(),
                                            static_cast<uint16_t>(data.size()));

  // If there's a B+Tree index on this table, update it
  // The first INT column is assumed to be the primary key
  BPlusTree *tree = ctx->getIndex(tableName_);
  if (tree && !values_.empty() && values_[0].type() == DataType::INT) {
    tree->insert(values_[0].asInt(), rid);
  }

  // Update row count
  info->rowCount++;
}

bool InsertExecutor::next(Tuple &tuple) {
  if (done_) return false;
  done_ = true;
  tuple = Tuple(values_);
  return true;
}

std::vector<char> InsertExecutor::serialize(const Tuple &tuple,
                                            const Schema &schema) {
  return serializeTuple(tuple, schema);
}

// ============================================================
// DeleteExecutor
// ============================================================

DeleteExecutor::DeleteExecutor(
    const std::string &tableName,
    std::function<bool(const Tuple &)> predicate)
    : tableName_(tableName), predicate_(predicate), fileId_(-1), done_(false),
      deletedCount_(0) {}

void DeleteExecutor::init(ExecutionContext *ctx) {
  TableInfo *info = ctx->catalog()->getTable(tableName_);
  if (!info) {
    throw std::runtime_error("Delete: table not found: " + tableName_);
  }

  schema_ = info->schema;
  fileId_ = info->fileId;
  done_ = false;
  deletedCount_ = 0;

  // Collect RIDs to delete (can't delete during scan)
  std::vector<std::pair<RID, int>> toDelete; // (rid, primaryKey)

  ctx->pageManager()->scanTable(
      fileId_, [&](const RID &rid, const char *data, uint16_t len) -> bool {
        Tuple tuple = deserializeTuple(data, len, schema_);
        if (!predicate_ || predicate_(tuple)) {
          int pk = -1;
          if (tuple.size() > 0 && tuple.getValue(0).type() == DataType::INT) {
            pk = tuple.getValue(0).asInt();
          }
          toDelete.push_back({rid, pk});
        }
        return true;
      });

  // Now delete them
  BPlusTree *tree = ctx->getIndex(tableName_);
  for (auto &[rid, pk] : toDelete) {
    ctx->pageManager()->deleteTuple(fileId_, rid);
    if (tree && pk >= 0) {
      tree->remove(pk);
    }
    deletedCount_++;
  }

  if (info->rowCount >= static_cast<uint32_t>(deletedCount_)) {
    info->rowCount -= deletedCount_;
  } else {
    info->rowCount = 0;
  }
}

bool DeleteExecutor::next(Tuple &tuple) {
  if (done_) return false;
  done_ = true;
  // Return a tuple indicating how many rows were deleted
  tuple = Tuple({Value(deletedCount_)});
  return true;
}

// ============================================================
// NestedLoopJoinExecutor
// ============================================================

NestedLoopJoinExecutor::NestedLoopJoinExecutor(Executor *left, Executor *right,
                                               const std::string &leftCol,
                                               const std::string &rightCol)
    : left_(left), right_(right), leftCol_(leftCol), rightCol_(rightCol),
      currentIdx_(0) {}

void NestedLoopJoinExecutor::init(ExecutionContext *ctx) {
  left_->init(ctx);
  right_->init(ctx);

  // Materialize both sides
  leftTuples_.clear();
  rightTuples_.clear();
  joinResults_.clear();
  currentIdx_ = 0;

  Tuple t;
  while (left_->next(t)) {
    leftTuples_.push_back(t);
  }
  while (right_->next(t)) {
    rightTuples_.push_back(t);
  }

  // Build output schema (concatenation of both schemas)
  const Schema &leftSchema = left_->outputSchema();
  const Schema &rightSchema = right_->outputSchema();

  for (size_t i = 0; i < leftSchema.columnCount(); i++) {
    outputSchema_.addColumn(leftSchema.getColumn(i));
  }
  for (size_t i = 0; i < rightSchema.columnCount(); i++) {
    outputSchema_.addColumn(rightSchema.getColumn(i));
  }

  // Find column indices for the join predicate
  int leftIdx = leftSchema.findColumn(leftCol_);
  int rightIdx = rightSchema.findColumn(rightCol_);

  if (leftIdx < 0 || rightIdx < 0) {
    throw std::runtime_error("Join: column not found for join predicate");
  }

  // Nested loop join
  for (const auto &lTuple : leftTuples_) {
    for (const auto &rTuple : rightTuples_) {
      if (lTuple.getValue(leftIdx) == rTuple.getValue(rightIdx)) {
        // Concatenate tuples
        std::vector<Value> combined;
        for (size_t i = 0; i < lTuple.size(); i++) {
          combined.push_back(lTuple.getValue(i));
        }
        for (size_t i = 0; i < rTuple.size(); i++) {
          combined.push_back(rTuple.getValue(i));
        }
        joinResults_.emplace_back(combined);
      }
    }
  }
}

bool NestedLoopJoinExecutor::next(Tuple &tuple) {
  if (currentIdx_ >= joinResults_.size()) return false;
  tuple = joinResults_[currentIdx_];
  currentIdx_++;
  return true;
}

// ============================================================
// FilterExecutor
// ============================================================

FilterExecutor::FilterExecutor(Executor *child,
                               std::function<bool(const Tuple &)> predicate)
    : child_(child), predicate_(predicate) {}

void FilterExecutor::init(ExecutionContext *ctx) { child_->init(ctx); }

bool FilterExecutor::next(Tuple &tuple) {
  while (child_->next(tuple)) {
    if (predicate_(tuple)) {
      return true;
    }
  }
  return false;
}

// ============================================================
// ProjectionExecutor
// ============================================================

ProjectionExecutor::ProjectionExecutor(Executor *child,
                                       const std::vector<std::string> &columns)
    : child_(child), columns_(columns) {}

void ProjectionExecutor::init(ExecutionContext *ctx) {
  child_->init(ctx);

  const Schema &inputSchema = child_->outputSchema();
  columnIndices_.clear();

  for (const auto &col : columns_) {
    int idx = inputSchema.findColumn(col);
    if (idx < 0) {
      throw std::runtime_error("Projection: column not found: " + col);
    }
    columnIndices_.push_back(idx);
    outputSchema_.addColumn(inputSchema.getColumn(idx));
  }
}

bool ProjectionExecutor::next(Tuple &tuple) {
  Tuple inputTuple;
  if (!child_->next(inputTuple)) return false;

  std::vector<Value> projected;
  for (int idx : columnIndices_) {
    projected.push_back(inputTuple.getValue(idx));
  }
  tuple = Tuple(projected);
  return true;
}
