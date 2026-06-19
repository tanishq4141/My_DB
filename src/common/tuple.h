#pragma once
#include "schema.h"
#include "value.h"
#include <iostream>
#include <stdexcept>
#include <vector>

// A tuple is an ordered list of values (one row of data, without metadata)
class Tuple {
public:
  Tuple() = default;
  Tuple(const std::vector<Value> &values) : values_(values) {}

  // Add a value
  void addValue(const Value &val) { values_.push_back(val); }

  // Number of values
  size_t size() const { return values_.size(); }

  // Access a value by index
  const Value &getValue(size_t index) const {
    if (index >= values_.size()) {
      throw std::out_of_range("Tuple index out of range");
    }
    return values_[index];
  }

  // Access a value by column name using a schema
  const Value &getValue(const std::string &columnName,
                        const Schema &schema) const {
    int idx = schema.findColumn(columnName);
    if (idx < 0) {
      throw std::runtime_error("Column not found: " + columnName);
    }
    return getValue(static_cast<size_t>(idx));
  }

  // Get all values
  const std::vector<Value> &values() const { return values_; }

  // Validate that this tuple matches a schema (correct count and types)
  bool matchesSchema(const Schema &schema) const {
    if (values_.size() != schema.columnCount()) return false;
    for (size_t i = 0; i < values_.size(); i++) {
      if (values_[i].type() != schema.getColumn(i).type) return false;
    }
    return true;
  }

  friend std::ostream &operator<<(std::ostream &os, const Tuple &tuple) {
    os << "(";
    for (size_t i = 0; i < tuple.values_.size(); i++) {
      os << tuple.values_[i];
      if (i + 1 < tuple.values_.size())
        os << ", ";
    }
    os << ")";
    return os;
  }

private:
  std::vector<Value> values_;
};
