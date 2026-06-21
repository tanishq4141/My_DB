#pragma once
#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Lock modes
enum class LockMode { SHARED, EXCLUSIVE };

// A resource identifier for locking (e.g. table + page or table + row)
struct ResourceId {
  std::string tableName;
  uint32_t pageId;
  uint16_t slotNum; // 0xFFFF means page-level lock

  ResourceId() : pageId(0), slotNum(0xFFFF) {}
  ResourceId(const std::string &table, uint32_t page, uint16_t slot = 0xFFFF)
      : tableName(table), pageId(page), slotNum(slot) {}

  bool operator==(const ResourceId &other) const {
    return tableName == other.tableName && pageId == other.pageId &&
           slotNum == other.slotNum;
  }
};

// Hash for ResourceId
namespace std {
template <> struct hash<ResourceId> {
  size_t operator()(const ResourceId &r) const {
    size_t h1 = hash<string>()(r.tableName);
    size_t h2 = hash<uint32_t>()(r.pageId);
    size_t h3 = hash<uint16_t>()(r.slotNum);
    return h1 ^ (h2 << 1) ^ (h3 << 2);
  }
};
} // namespace std

// A lock request in the lock table
struct LockRequest {
  int txnId;
  LockMode mode;
  bool granted;
};

// LockManager implements strict two-phase locking (2PL).
// Supports shared and exclusive locks at page or row level.
// Includes simple deadlock detection via wait-for graph.
class LockManager {
public:
  LockManager();

  // Acquire a shared lock on a resource. Blocks if an exclusive lock is held.
  bool lockShared(int txnId, const ResourceId &res);

  // Acquire an exclusive lock. Blocks if any lock is held by another txn.
  bool lockExclusive(int txnId, const ResourceId &res);

  // Release a specific lock
  void unlock(int txnId, const ResourceId &res);

  // Release all locks held by a transaction (called on commit/abort)
  void unlockAll(int txnId);

  // Check if a deadlock exists (returns true if cycle detected)
  bool hasDeadlock(int txnId);

private:
  // Lock table: resource -> list of lock requests
  std::unordered_map<ResourceId, std::vector<LockRequest>> lockTable_;

  // Condition variable for blocking lock requests
  std::condition_variable cv_;
  std::mutex mutex_;

  // Wait-for graph: txnId -> set of txnIds it's waiting for
  std::unordered_map<int, std::unordered_set<int>> waitForGraph_;

  // Detect cycle in wait-for graph using DFS
  bool detectCycle(int txnId, std::unordered_set<int> &visited,
                   std::unordered_set<int> &inStack);
};
