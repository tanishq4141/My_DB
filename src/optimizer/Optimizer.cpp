#include "Optimizer.h"
#include <cmath>
#include <iostream>

Optimizer::Optimizer(Catalog *catalog) : catalog_(catalog) {}

PlanNode *Optimizer::optimize(const Statement &stmt) {
  switch (stmt.type) {
  case StatementType::SELECT:
    return optimizeSelect(stmt);
  case StatementType::INSERT:
    return optimizeInsert(stmt);
  case StatementType::DELETE_STMT:
    return optimizeDelete(stmt);
  case StatementType::CREATE_TABLE:
    // CREATE TABLE doesn't need a query plan
    return nullptr;
  }
  return nullptr;
}

PlanNode *Optimizer::optimizeSelect(const Statement &stmt) {
  PlanNode *scanNode = new PlanNode();

  // Decide: index scan or sequential scan?
  if (stmt.hasWhere &&
      shouldUseIndex(stmt.tableName, stmt.where.column)) {
    // Use index scan for the WHERE predicate
    scanNode->type = PlanNodeType::INDEX_SCAN;
    scanNode->tableName = stmt.tableName;
    scanNode->hasPredicate = true;
    scanNode->predicateCol = stmt.where.column;
    scanNode->predicateVal = stmt.where.value;

    double sel = estimateSelectivity(stmt.tableName, stmt.where.column,
                                     stmt.where.value);
    const TableInfo *info = catalog_->getTable(stmt.tableName);
    scanNode->estimatedCost = std::log2(info ? info->rowCount + 1 : 1) + 1;
    scanNode->estimatedRows = (info ? info->rowCount : 0) * sel;
  } else {
    // Sequential scan
    scanNode->type = PlanNodeType::SEQ_SCAN;
    scanNode->tableName = stmt.tableName;
    scanNode->hasPredicate = stmt.hasWhere;
    if (stmt.hasWhere) {
      scanNode->predicateCol = stmt.where.column;
      scanNode->predicateVal = stmt.where.value;
    }
    scanNode->estimatedCost = estimateScanCost(stmt.tableName);

    const TableInfo *info = catalog_->getTable(stmt.tableName);
    if (stmt.hasWhere) {
      double sel = estimateSelectivity(stmt.tableName, stmt.where.column,
                                       stmt.where.value);
      scanNode->estimatedRows = (info ? info->rowCount : 0) * sel;
    } else {
      scanNode->estimatedRows = info ? info->rowCount : 0;
    }
  }

  // Add projection if specific columns are requested
  if (!stmt.selectAll && !stmt.selectColumns.empty()) {
    PlanNode *projNode = new PlanNode();
    projNode->type = PlanNodeType::PROJECTION;
    projNode->outputColumns = stmt.selectColumns;
    projNode->left = scanNode;
    projNode->estimatedCost = scanNode->estimatedCost + 0.1;
    projNode->estimatedRows = scanNode->estimatedRows;
    return projNode;
  }

  scanNode->selectAll = stmt.selectAll;
  return scanNode;
}

PlanNode *Optimizer::optimizeInsert(const Statement &stmt) {
  PlanNode *node = new PlanNode();
  node->type = PlanNodeType::INSERT_OP;
  node->tableName = stmt.tableName;
  node->insertValues = stmt.values;
  node->estimatedCost = 1.0;
  node->estimatedRows = 1.0;
  return node;
}

PlanNode *Optimizer::optimizeDelete(const Statement &stmt) {
  // Build a scan node first, then wrap with delete
  PlanNode *scanNode = new PlanNode();
  scanNode->type = PlanNodeType::SEQ_SCAN;
  scanNode->tableName = stmt.tableName;
  scanNode->hasPredicate = stmt.hasWhere;
  if (stmt.hasWhere) {
    scanNode->predicateCol = stmt.where.column;
    scanNode->predicateVal = stmt.where.value;
  }
  scanNode->estimatedCost = estimateScanCost(stmt.tableName);

  PlanNode *deleteNode = new PlanNode();
  deleteNode->type = PlanNodeType::DELETE_OP;
  deleteNode->tableName = stmt.tableName;
  deleteNode->hasPredicate = stmt.hasWhere;
  if (stmt.hasWhere) {
    deleteNode->predicateCol = stmt.where.column;
    deleteNode->predicateVal = stmt.where.value;
  }
  deleteNode->left = scanNode;
  deleteNode->estimatedCost = scanNode->estimatedCost + 1.0;

  const TableInfo *info = catalog_->getTable(stmt.tableName);
  if (stmt.hasWhere) {
    double sel = estimateSelectivity(stmt.tableName, stmt.where.column,
                                     stmt.where.value);
    deleteNode->estimatedRows = (info ? info->rowCount : 0) * sel;
  } else {
    deleteNode->estimatedRows = info ? info->rowCount : 0;
  }

  return deleteNode;
}

double Optimizer::estimateScanCost(const std::string &tableName) {
  const TableInfo *info = catalog_->getTable(tableName);
  if (!info) return 1.0;

  // Cost is proportional to number of pages
  // Assume ~10 tuples per page as a rough estimate
  double pages = std::ceil(info->rowCount / 10.0);
  return std::max(pages, 1.0);
}

double Optimizer::estimateSelectivity(const std::string &tableName,
                                      const std::string &column,
                                      const std::string &value) {
  // Simple heuristic: assume uniform distribution
  // Equality selectivity = 1 / distinct_values
  // Without real statistics, we estimate 1/10 for equality predicates
  const TableInfo *info = catalog_->getTable(tableName);
  if (!info || info->rowCount == 0) return 0.1;

  // If few rows, selectivity is higher
  if (info->rowCount < 10) {
    return 1.0 / info->rowCount;
  }

  return 0.1; // default 10% selectivity
}

bool Optimizer::shouldUseIndex(const std::string &tableName,
                               const std::string &column) {
  const TableInfo *info = catalog_->getTable(tableName);
  if (!info || !info->hasIndex) return false;

  // Use index only if the predicate is on the first (primary key) column
  // and the table is large enough to benefit
  if (info->schema.columnCount() > 0 &&
      info->schema.getColumn(0).name == column &&
      info->rowCount > 50) {
    return true;
  }

  return false;
}
