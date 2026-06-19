#pragma once
#include <iostream>
#include <string>
#include <variant>

// Supported column data types
enum class DataType { INT, TEXT };

// A single value in the database — can hold an int or a string
class Value {
public:
  Value() : data_(""), type_(DataType::TEXT) {}
  Value(int val) : data_(val), type_(DataType::INT) {}
  Value(const std::string &val) : data_(val), type_(DataType::TEXT) {}

  DataType type() const { return type_; }

  int asInt() const { return std::get<int>(data_); }
  const std::string &asText() const { return std::get<std::string>(data_); }

  // Get a string representation regardless of type
  std::string toString() const {
    if (type_ == DataType::INT) {
      return std::to_string(asInt());
    }
    return asText();
  }

  // Equality comparison
  bool operator==(const Value &other) const {
    if (type_ != other.type_) return false;
    return data_ == other.data_;
  }

  bool operator!=(const Value &other) const { return !(*this == other); }

  friend std::ostream &operator<<(std::ostream &os, const Value &val) {
    os << val.toString();
    return os;
  }

private:
  std::variant<int, std::string> data_;
  DataType type_;
};

// Helper to convert DataType enum to string
inline std::string dataTypeName(DataType dt) {
  switch (dt) {
  case DataType::INT: return "INT";
  case DataType::TEXT: return "TEXT";
  }
  return "UNKNOWN";
}
