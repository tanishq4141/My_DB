#pragma once
#include "tuple.h"
#include <cstdint>
#include <iostream>

// A row is a tuple with a unique row ID (used for storage and WHERE filtering)
class Row {
public:
  Row() : id_(0) {}
  Row(uint32_t id, const Tuple &data) : id_(id), data_(data) {}

  uint32_t id() const { return id_; }
  const Tuple &data() const { return data_; }
  Tuple &data() { return data_; }

  friend std::ostream &operator<<(std::ostream &os, const Row &row) {
    os << "Row#" << row.id_ << " " << row.data_;
    return os;
  }

private:
  uint32_t id_; // unique row identifier
  Tuple data_;  // the actual column values
};
