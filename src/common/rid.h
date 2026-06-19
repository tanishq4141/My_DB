#pragma once
#include <cstdint>
#include <functional>
#include <iostream>

// Record Identifier: uniquely identifies a row within a table
// Composed of a page ID and a slot number within that page
struct RID {
  uint32_t pageId;
  uint16_t slotNum;

  RID() : pageId(0), slotNum(0) {}
  RID(uint32_t pid, uint16_t slot) : pageId(pid), slotNum(slot) {}

  bool operator==(const RID &other) const {
    return pageId == other.pageId && slotNum == other.slotNum;
  }

  bool operator!=(const RID &other) const { return !(*this == other); }

  bool operator<(const RID &other) const {
    if (pageId != other.pageId) return pageId < other.pageId;
    return slotNum < other.slotNum;
  }

  friend std::ostream &operator<<(std::ostream &os, const RID &rid) {
    os << "(" << rid.pageId << "," << rid.slotNum << ")";
    return os;
  }
};

// Hash support for RID (for use in unordered_map/set)
namespace std {
template <> struct hash<RID> {
  size_t operator()(const RID &rid) const {
    return hash<uint64_t>()(((uint64_t)rid.pageId << 16) | rid.slotNum);
  }
};
} // namespace std
