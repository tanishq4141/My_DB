#include "LockManager.h"
#include <iostream>
#include <stdexcept>

LockManager::LockManager() {}

bool LockManager::lockShared(int txnId, const ResourceId &res) {
  std::unique_lock<std::mutex> lock(mutex_);

  auto &requests = lockTable_[res];

  // Check if this txn already holds a lock on this resource
  for (auto &req : requests) {
    if (req.txnId == txnId && req.granted) {
      return true; // already have a lock (upgrade not implemented)
    }
  }

  // Check if there's an exclusive lock held by another transaction
  bool canGrant = true;
  for (auto &req : requests) {
    if (req.txnId != txnId && req.mode == LockMode::EXCLUSIVE && req.granted) {
      canGrant = false;
      break;
    }
  }

  if (canGrant) {
    requests.push_back({txnId, LockMode::SHARED, true});
    return true;
  }

  // Must wait — add to wait-for graph
  for (auto &req : requests) {
    if (req.txnId != txnId && req.granted) {
      waitForGraph_[txnId].insert(req.txnId);
    }
  }

  // Check for deadlock
  if (hasDeadlock(txnId)) {
    waitForGraph_.erase(txnId);
    return false; // caller should abort this transaction
  }

  // Add pending request and wait
  requests.push_back({txnId, LockMode::SHARED, false});
  auto &pendingReq = requests.back();

  cv_.wait(lock, [&]() {
    // Re-check if we can be granted
    for (auto &r : lockTable_[res]) {
      if (r.txnId != txnId && r.mode == LockMode::EXCLUSIVE && r.granted) {
        return false;
      }
    }
    return true;
  });

  // Grant the lock
  for (auto &r : requests) {
    if (r.txnId == txnId && !r.granted) {
      r.granted = true;
      break;
    }
  }

  waitForGraph_.erase(txnId);
  return true;
}

bool LockManager::lockExclusive(int txnId, const ResourceId &res) {
  std::unique_lock<std::mutex> lock(mutex_);

  auto &requests = lockTable_[res];

  // Check if this txn already holds an exclusive lock
  for (auto &req : requests) {
    if (req.txnId == txnId && req.granted &&
        req.mode == LockMode::EXCLUSIVE) {
      return true;
    }
  }

  // Check if any other transaction holds any lock
  bool canGrant = true;
  for (auto &req : requests) {
    if (req.txnId != txnId && req.granted) {
      canGrant = false;
      break;
    }
  }

  if (canGrant) {
    requests.push_back({txnId, LockMode::EXCLUSIVE, true});
    return true;
  }

  // Must wait
  for (auto &req : requests) {
    if (req.txnId != txnId && req.granted) {
      waitForGraph_[txnId].insert(req.txnId);
    }
  }

  if (hasDeadlock(txnId)) {
    waitForGraph_.erase(txnId);
    return false;
  }

  requests.push_back({txnId, LockMode::EXCLUSIVE, false});

  cv_.wait(lock, [&]() {
    for (auto &r : lockTable_[res]) {
      if (r.txnId != txnId && r.granted) {
        return false;
      }
    }
    return true;
  });

  for (auto &r : requests) {
    if (r.txnId == txnId && !r.granted) {
      r.granted = true;
      break;
    }
  }

  waitForGraph_.erase(txnId);
  return true;
}

void LockManager::unlock(int txnId, const ResourceId &res) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = lockTable_.find(res);
  if (it == lockTable_.end()) return;

  auto &requests = it->second;
  for (auto reqIt = requests.begin(); reqIt != requests.end(); ++reqIt) {
    if (reqIt->txnId == txnId && reqIt->granted) {
      requests.erase(reqIt);
      break;
    }
  }

  // Remove empty entries
  if (requests.empty()) {
    lockTable_.erase(it);
  }

  // Notify waiting threads
  cv_.notify_all();
}

void LockManager::unlockAll(int txnId) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Collect resources to unlock (can't modify while iterating)
  std::vector<ResourceId> toUnlock;

  for (auto &[res, requests] : lockTable_) {
    for (auto &req : requests) {
      if (req.txnId == txnId) {
        toUnlock.push_back(res);
        break;
      }
    }
  }

  for (auto &res : toUnlock) {
    auto &requests = lockTable_[res];
    requests.erase(
        std::remove_if(requests.begin(), requests.end(),
                        [txnId](const LockRequest &r) {
                          return r.txnId == txnId;
                        }),
        requests.end());

    if (requests.empty()) {
      lockTable_.erase(res);
    }
  }

  waitForGraph_.erase(txnId);
  cv_.notify_all();
}

bool LockManager::hasDeadlock(int txnId) {
  std::unordered_set<int> visited;
  std::unordered_set<int> inStack;
  return detectCycle(txnId, visited, inStack);
}

bool LockManager::detectCycle(int txnId, std::unordered_set<int> &visited,
                               std::unordered_set<int> &inStack) {
  visited.insert(txnId);
  inStack.insert(txnId);

  auto it = waitForGraph_.find(txnId);
  if (it != waitForGraph_.end()) {
    for (int neighbor : it->second) {
      if (inStack.count(neighbor)) {
        return true; // cycle detected
      }
      if (!visited.count(neighbor)) {
        if (detectCycle(neighbor, visited, inStack)) {
          return true;
        }
      }
    }
  }

  inStack.erase(txnId);
  return false;
}
