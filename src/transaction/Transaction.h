#pragma once
#include "LockManager.h"
#include <cstdint>
#include <unordered_set>
#include <vector>

// Transaction states
enum class TxnState { ACTIVE, COMMITTED, ABORTED };

// A transaction tracks its state and acquired locks
class Transaction {
public:
  explicit Transaction(int txnId);

  int id() const { return txnId_; }
  TxnState state() const { return state_; }

  void setState(TxnState state) { state_ = state; }

  // Track which resources this transaction has locked
  void addLock(const ResourceId &res) { lockedResources_.insert(res); }
  const std::unordered_set<ResourceId> &lockedResources() const {
    return lockedResources_;
  }

private:
  int txnId_;
  TxnState state_;
  std::unordered_set<ResourceId> lockedResources_;
};

// TransactionManager coordinates begin/commit/abort of transactions
class TransactionManager {
public:
  TransactionManager(LockManager *lockMgr);

  // Begin a new transaction, returns the transaction ID
  int begin();

  // Commit a transaction (release all locks)
  void commit(int txnId);

  // Abort a transaction (release all locks)
  void abort(int txnId);

  // Get a transaction by ID
  Transaction *getTransaction(int txnId);

private:
  LockManager *lockMgr_;
  std::unordered_map<int, Transaction> transactions_;
  int nextTxnId_;
  std::mutex mutex_;
};
