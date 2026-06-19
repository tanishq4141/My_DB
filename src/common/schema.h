#pragma once
#include "column.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// A schema describes the structure of a table: an ordered list of columns
class Schema {
public:
  Schema() = default;
  Schema(const std::vector<Column> &columns) : columns_(columns) {}

  // Add a column to the schema
  void addColumn(const Column &col) { columns_.push_back(col); }
  void addColumn(const std::string &name, DataType type) {
    columns_.emplace_back(name, type);
  }

  // Number of columns
  size_t columnCount() const { return columns_.size(); }

  // Access a column by index
  const Column &getColumn(size_t index) const {
    if (index >= columns_.size()) {
      throw std::out_of_range("Column index out of range");
    }
    return columns_[index];
  }

  // Find a column index by name, returns -1 if not found
  int findColumn(const std::string &name) const {
    for (size_t i = 0; i < columns_.size(); i++) {
      if (columns_[i].name == name) {
        return static_cast<int>(i);
      }
    }
    return -1;
  }

  // Get all columns
  const std::vector<Column> &columns() const { return columns_; }

  friend std::ostream &operator<<(std::ostream &os, const Schema &schema) {
    os << "(";
    for (size_t i = 0; i < schema.columns_.size(); i++) {
      os << schema.columns_[i];
      if (i + 1 < schema.columns_.size())
        os << ", ";
    }
    os << ")";
    return os;
  }

private:
  std::vector<Column> columns_;
};
