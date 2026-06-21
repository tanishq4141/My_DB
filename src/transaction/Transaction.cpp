#include "Transaction.h"
#include <iostream>

// ============================================================
// Transaction
// ============================================================

Transaction::Transaction(int txnId)
    : txnId_(txnId), state_(TxnState::ACTIVE) {}

// ============================================================
// TransactionManager
// ============================================================

TransactionManager::TransactionManager(LockManager *lockMgr)
    : lockMgr_(lockMgr), nextTxnId_(1) {}

int TransactionManager::begin() {
  std::lock_guard<std::mutex> lock(mutex_);
  int txnId = nextTxnId_++;
  transactions_.emplace(txnId, Transaction(txnId));
  return txnId;
}

void TransactionManager::commit(int txnId) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = transactions_.find(txnId);
  if (it == transactions_.end()) return;

  it->second.setState(TxnState::COMMITTED);

  // Release all locks (shrinking phase of 2PL)
  lockMgr_->unlockAll(txnId);
}

void TransactionManager::abort(int txnId) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = transactions_.find(txnId);
  if (it == transactions_.end()) return;

  it->second.setState(TxnState::ABORTED);

  // Release all locks
  lockMgr_->unlockAll(txnId);
}

Transaction *TransactionManager::getTransaction(int txnId) {
  auto it = transactions_.find(txnId);
  if (it == transactions_.end()) return nullptr;
  return &it->second;
}
