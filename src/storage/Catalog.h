#pragma once
#include "../common/schema.h"
#include <string>
#include <unordered_map>

// Metadata for a single table in the catalog
struct TableInfo {
  std::string name;
  Schema schema;
  int fileId;        // file ID in DiskManager
  uint32_t rowCount; // approximate row count (for optimizer)
  bool hasIndex;     // whether a B+ tree index exists on primary key
  int indexFileId;   // file ID for the index (if hasIndex)
};

// The Catalog maintains metadata about all tables in the database.
// It maps table names to their schemas, file IDs, and statistics.
class Catalog {
public:
  Catalog() = default;

  // Register a new table
  void addTable(const std::string &name, const Schema &schema, int fileId) {
    TableInfo info;
    info.name = name;
    info.schema = schema;
    info.fileId = fileId;
    info.rowCount = 0;
    info.hasIndex = false;
    info.indexFileId = -1;
    tables_[name] = info;
  }

  // Look up a table by name. Returns nullptr if not found.
  TableInfo *getTable(const std::string &name) {
    auto it = tables_.find(name);
    if (it == tables_.end()) return nullptr;
    return &it->second;
  }

  const TableInfo *getTable(const std::string &name) const {
    auto it = tables_.find(name);
    if (it == tables_.end()) return nullptr;
    return &it->second;
  }

  // Check if a table exists
  bool tableExists(const std::string &name) const {
    return tables_.count(name) > 0;
  }

  // Remove a table from the catalog
  void dropTable(const std::string &name) { tables_.erase(name); }

  // Update row count (for optimizer statistics)
  void updateRowCount(const std::string &name, uint32_t count) {
    auto it = tables_.find(name);
    if (it != tables_.end()) {
      it->second.rowCount = count;
    }
  }

  // Mark that an index exists for a table
  void setIndex(const std::string &name, int indexFileId) {
    auto it = tables_.find(name);
    if (it != tables_.end()) {
      it->second.hasIndex = true;
      it->second.indexFileId = indexFileId;
    }
  }

  // Get all table names
  std::vector<std::string> getTableNames() const {
    std::vector<std::string> names;
    for (const auto &[name, _] : tables_) {
      names.push_back(name);
    }
    return names;
  }

private:
  std::unordered_map<std::string, TableInfo> tables_;
};
