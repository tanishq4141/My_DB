#pragma once
#include "value.h"
#include <string>

// A column definition: name + data type
struct Column {
  std::string name;
  DataType type;

  Column() : name(""), type(DataType::TEXT) {}
  Column(const std::string &name, DataType type) : name(name), type(type) {}

  friend std::ostream &operator<<(std::ostream &os, const Column &col) {
    os << col.name << " " << dataTypeName(col.type);
    return os;
  }
};
