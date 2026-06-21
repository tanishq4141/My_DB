#include "common/rid.h"
#include "common/value.h"
#include "index/BPlusTree.h"
#include "optimizer/Optimizer.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "query/Executor.h"
#include "recovery/LogManager.h"
#include "recovery/Recovery.h"
#include "storage/BufferPool.h"
#include "storage/Catalog.h"
#include "storage/DiskManager.h"
#include "storage/PageManager.h"
#include "transaction/LockManager.h"
#include "transaction/Transaction.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

// ============================================================
// MiniDB Engine: ties all subsystems together
// ============================================================
class MiniDB {
public:
  MiniDB()
      : diskMgr_(), bufferPool_(&diskMgr_, 256), pageMgr_(&diskMgr_, &bufferPool_),
        catalog_(), optimizer_(&catalog_), lockMgr_(), txnMgr_(&lockMgr_),
        logMgr_() {
    logMgr_.open();
    execCtx_ = new ExecutionContext(&catalog_, &pageMgr_, &diskMgr_, &bufferPool_);
  }

  ~MiniDB() {
    bufferPool_.flushAllPages();
    delete execCtx_;
  }

  void execute(const std::string &sql) {
    Lexer lexer;
    Parser parser;

    std::vector<Token> tokens = lexer.handleInput(sql);

    Statement stmt;
    try {
      stmt = parser.parse(tokens);
    } catch (const std::runtime_error &e) {
      std::cerr << "Parse Error: " << e.what() << std::endl;
      return;
    }

    // Handle CREATE TABLE directly (no optimizer needed)
    if (stmt.type == StatementType::CREATE_TABLE) {
      executeCreateTable(stmt);
      return;
    }

    // Use optimizer to build query plan
    PlanNode *plan = optimizer_.optimize(stmt);
    if (!plan) {
      std::cerr << "Error: optimizer returned no plan." << std::endl;
      return;
    }

    // Execute the plan
    executePlan(plan, stmt);

    delete plan;
  }

private:
  DiskManager diskMgr_;
  BufferPool bufferPool_;
  PageManager pageMgr_;
  Catalog catalog_;
  Optimizer optimizer_;
  LockManager lockMgr_;
  TransactionManager txnMgr_;
  LogManager logMgr_;
  ExecutionContext *execCtx_;

  // Per-table B+ tree indexes
  std::unordered_map<std::string, BPlusTree *> indexes_;

  void executeCreateTable(const Statement &stmt) {
    if (catalog_.tableExists(stmt.tableName)) {
      std::cerr << "Error: table '" << stmt.tableName << "' already exists."
                << std::endl;
      return;
    }

    // Build schema from column definitions
    Schema schema;
    for (const auto &col : stmt.columns) {
      DataType dt = (col.type == "INT") ? DataType::INT : DataType::TEXT;
      schema.addColumn(col.name, dt);
    }

    // Create the heap file
    int fileId = pageMgr_.createTable(stmt.tableName);
    catalog_.addTable(stmt.tableName, schema, fileId);

    // Create a B+ tree index on the first column (primary key)
    BPlusTree *tree = new BPlusTree(128);
    indexes_[stmt.tableName] = tree;
    execCtx_->setIndex(stmt.tableName, tree);

    std::cout << "Table '" << stmt.tableName << "' created with "
              << schema.columnCount() << " columns." << std::endl;
  }

  void executePlan(PlanNode *plan, const Statement &stmt) {
    switch (plan->type) {
    case PlanNodeType::INSERT_OP:
      executeInsert(plan, stmt);
      break;
    case PlanNodeType::DELETE_OP:
      executeDelete(plan, stmt);
      break;
    case PlanNodeType::SEQ_SCAN:
    case PlanNodeType::INDEX_SCAN:
      executeSelect(plan, stmt);
      break;
    case PlanNodeType::PROJECTION:
      executeSelect(plan, stmt);
      break;
    default:
      std::cerr << "Error: unsupported plan node type." << std::endl;
    }
  }

  void executeInsert(PlanNode * /*plan*/, const Statement &stmt) {
    TableInfo *info = catalog_.getTable(stmt.tableName);
    if (!info) {
      std::cerr << "Error: table '" << stmt.tableName << "' does not exist."
                << std::endl;
      return;
    }

    // Convert string values to typed Values based on schema
    std::vector<Value> values;
    const Schema &schema = info->schema;

    if (stmt.values.size() != schema.columnCount()) {
      std::cerr << "Error: expected " << schema.columnCount() << " values, got "
                << stmt.values.size() << "." << std::endl;
      return;
    }

    for (size_t i = 0; i < stmt.values.size(); i++) {
      if (schema.getColumn(i).type == DataType::INT) {
        try {
          values.emplace_back(std::stoi(stmt.values[i]));
        } catch (...) {
          std::cerr << "Error: expected integer for column '"
                    << schema.getColumn(i).name << "'." << std::endl;
          return;
        }
      } else {
        values.emplace_back(stmt.values[i]);
      }
    }

    // Begin transaction
    int txnId = txnMgr_.begin();
    logMgr_.logBegin(txnId);

    InsertExecutor executor(stmt.tableName, values);
    executor.init(execCtx_);

    Tuple result;
    executor.next(result);

    // Commit
    logMgr_.logCommit(txnId);
    txnMgr_.commit(txnId);

    // Update catalog statistics
    info->rowCount++;

    std::cout << "Inserted 1 row into '" << stmt.tableName << "'." << std::endl;
  }

  void executeSelect(PlanNode * /*plan*/, const Statement &stmt) {
    TableInfo *info = catalog_.getTable(stmt.tableName);
    if (!info) {
      std::cerr << "Error: table '" << stmt.tableName << "' does not exist."
                << std::endl;
      return;
    }

    const Schema &schema = info->schema;

    // Build a predicate function if WHERE clause exists
    std::function<bool(const Tuple &)> predicate = nullptr;
    if (stmt.hasWhere) {
      int colIdx = schema.findColumn(stmt.where.column);
      if (colIdx < 0) {
        std::cerr << "Error: column '" << stmt.where.column << "' not found."
                  << std::endl;
        return;
      }

      DataType colType = schema.getColumn(colIdx).type;
      std::string whereVal = stmt.where.value;
      predicate = [colIdx, colType, whereVal](const Tuple &t) -> bool {
        if (static_cast<size_t>(colIdx) >= t.size()) return false;
        const Value &v = t.getValue(colIdx);
        if (colType == DataType::INT) {
          try {
            return v.asInt() == std::stoi(whereVal);
          } catch (...) {
            return false;
          }
        }
        return v.asText() == whereVal;
      };
    }

    SeqScanExecutor scanExec(stmt.tableName, predicate);
    scanExec.init(execCtx_);

    // Determine output columns
    std::vector<std::string> outputCols;
    std::vector<int> outputIndices;

    if (stmt.selectAll || stmt.selectColumns.empty()) {
      for (size_t i = 0; i < schema.columnCount(); i++) {
        outputCols.push_back(schema.getColumn(i).name);
        outputIndices.push_back(static_cast<int>(i));
      }
    } else {
      for (const auto &col : stmt.selectColumns) {
        int idx = schema.findColumn(col);
        if (idx < 0) {
          std::cerr << "Error: column '" << col << "' not found." << std::endl;
          return;
        }
        outputCols.push_back(col);
        outputIndices.push_back(idx);
      }
    }

    // Print header
    std::cout << "+";
    for (size_t i = 0; i < outputCols.size(); i++) {
      std::cout << std::string(15, '-') << "+";
    }
    std::cout << std::endl << "|";
    for (const auto &col : outputCols) {
      std::cout << std::setw(15) << std::left << col << "|";
    }
    std::cout << std::endl << "+";
    for (size_t i = 0; i < outputCols.size(); i++) {
      std::cout << std::string(15, '-') << "+";
    }
    std::cout << std::endl;

    // Print rows
    Tuple tuple;
    int rowCount = 0;
    while (scanExec.next(tuple)) {
      std::cout << "|";
      for (int idx : outputIndices) {
        if (static_cast<size_t>(idx) < tuple.size()) {
          std::cout << std::setw(15) << std::left << tuple.getValue(idx).toString() << "|";
        }
      }
      std::cout << std::endl;
      rowCount++;
    }

    std::cout << "+";
    for (size_t i = 0; i < outputCols.size(); i++) {
      std::cout << std::string(15, '-') << "+";
    }
    std::cout << std::endl;
    std::cout << rowCount << " row(s) returned." << std::endl;
  }

  void executeDelete(PlanNode * /*plan*/, const Statement &stmt) {
    TableInfo *info = catalog_.getTable(stmt.tableName);
    if (!info) {
      std::cerr << "Error: table '" << stmt.tableName << "' does not exist."
                << std::endl;
      return;
    }

    const Schema &schema = info->schema;

    // Build predicate
    std::function<bool(const Tuple &)> predicate = nullptr;
    if (stmt.hasWhere) {
      int colIdx = schema.findColumn(stmt.where.column);
      if (colIdx < 0) {
        std::cerr << "Error: column '" << stmt.where.column << "' not found."
                  << std::endl;
        return;
      }

      DataType colType = schema.getColumn(colIdx).type;
      std::string whereVal = stmt.where.value;
      predicate = [colIdx, colType, whereVal](const Tuple &t) -> bool {
        if (static_cast<size_t>(colIdx) >= t.size()) return false;
        const Value &v = t.getValue(colIdx);
        if (colType == DataType::INT) {
          try {
            return v.asInt() == std::stoi(whereVal);
          } catch (...) {
            return false;
          }
        }
        return v.asText() == whereVal;
      };
    }

    int txnId = txnMgr_.begin();
    logMgr_.logBegin(txnId);

    DeleteExecutor delExec(stmt.tableName, predicate);
    delExec.init(execCtx_);

    Tuple result;
    delExec.next(result);

    logMgr_.logCommit(txnId);
    txnMgr_.commit(txnId);

    int deleted = result.getValue(0).asInt();
    std::cout << "Deleted " << deleted << " row(s) from '" << stmt.tableName
              << "'." << std::endl;
  }
};

// ============================================================
// Main REPL
// ============================================================
int main() {
  MiniDB db;

  std::cout << "╔══════════════════════════════════════════╗" << std::endl;
  std::cout << "║          MiniDB v1.0 — SQL Engine        ║" << std::endl;
  std::cout << "║  Type SQL commands or '.exit' to quit    ║" << std::endl;
  std::cout << "╚══════════════════════════════════════════╝" << std::endl;
  std::cout << std::endl;

  std::string input;
  while (true) {
    std::cout << "minidb> ";
    std::getline(std::cin, input);

    if (input == ".exit" || input == "quit" || input == "exit") {
      std::cout << "Goodbye!" << std::endl;
      break;
    }

    if (input.empty()) continue;

    if (input == ".help") {
      std::cout << "Supported commands:" << std::endl;
      std::cout << "  CREATE TABLE name (col1 TYPE, col2 TYPE, ...);" << std::endl;
      std::cout << "  INSERT INTO name VALUES (val1, val2, ...);" << std::endl;
      std::cout << "  SELECT * FROM name [WHERE col = val];" << std::endl;
      std::cout << "  SELECT col1, col2 FROM name [WHERE col = val];" << std::endl;
      std::cout << "  DELETE FROM name [WHERE col = val];" << std::endl;
      std::cout << "  .help  — show this help" << std::endl;
      std::cout << "  .exit  — quit MiniDB" << std::endl;
      continue;
    }

    db.execute(input);
  }

  return 0;
}
