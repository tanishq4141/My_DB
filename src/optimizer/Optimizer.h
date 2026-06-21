#pragma once
#include "../parser/parser.h"
#include "../storage/Catalog.h"
#include <string>
#include <vector>

// Types of physical operators in the query plan
enum class PlanNodeType {
  SEQ_SCAN,
  INDEX_SCAN,
  NESTED_LOOP_JOIN,
  HASH_JOIN,
  FILTER,
  PROJECTION,
  INSERT_OP,
  DELETE_OP
};

// A node in the physical query plan tree
struct PlanNode {
  PlanNodeType type;
  std::string tableName;

  // For scans: column and value for predicate
  std::string predicateCol;
  std::string predicateVal;
  bool hasPredicate;

  // For joins: join columns
  std::string leftJoinCol;
  std::string rightJoinCol;

  // For projection: which columns to output
  std::vector<std::string> outputColumns;
  bool selectAll;

  // For inserts: the values to insert
  std::vector<std::string> insertValues;

  // Children plan nodes
  PlanNode *left;
  PlanNode *right;

  // Cost estimate from the optimizer
  double estimatedCost;
  double estimatedRows;

  PlanNode()
      : type(PlanNodeType::SEQ_SCAN), hasPredicate(false), selectAll(false),
        left(nullptr), right(nullptr), estimatedCost(0.0),
        estimatedRows(0.0) {}

  ~PlanNode() {
    delete left;
    delete right;
  }
};

// The Optimizer transforms a parsed AST (Statement) into a physical
// query plan (PlanNode tree), using cost-based heuristics.
class Optimizer {
public:
  Optimizer(Catalog *catalog);

  // Optimize a parsed statement into a query plan
  PlanNode *optimize(const Statement &stmt);

private:
  Catalog *catalog_;

  // Build plan for different statement types
  PlanNode *optimizeSelect(const Statement &stmt);
  PlanNode *optimizeInsert(const Statement &stmt);
  PlanNode *optimizeDelete(const Statement &stmt);

  // Cost estimation
  double estimateScanCost(const std::string &tableName);
  double estimateSelectivity(const std::string &tableName,
                             const std::string &column,
                             const std::string &value);

  // Decide whether to use index scan vs sequential scan
  bool shouldUseIndex(const std::string &tableName,
                      const std::string &column);
};
