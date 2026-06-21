#include "Recovery.h"
#include <iostream>

RecoveryManager::RecoveryManager(LogManager *logMgr, PageManager *pageMgr)
    : logMgr_(logMgr), pageMgr_(pageMgr) {}

void RecoveryManager::recover() {
  std::cout << "[Recovery] Starting crash recovery..." << std::endl;

  // Read all log records
  std::vector<LogRecord> records = logMgr_->readAllRecords();

  if (records.empty()) {
    std::cout << "[Recovery] No log records found, nothing to recover."
              << std::endl;
    return;
  }

  std::cout << "[Recovery] Found " << records.size() << " log records."
            << std::endl;

  // Analysis phase: determine which transactions committed and which didn't
  std::unordered_set<int> activeTxns;
  std::unordered_set<int> committedTxns;
  std::unordered_set<int> abortedTxns;

  for (const auto &rec : records) {
    switch (rec.type) {
    case LogRecordType::BEGIN_TXN:
      activeTxns.insert(rec.txnId);
      break;
    case LogRecordType::COMMIT_TXN:
      activeTxns.erase(rec.txnId);
      committedTxns.insert(rec.txnId);
      break;
    case LogRecordType::ABORT_TXN:
      activeTxns.erase(rec.txnId);
      abortedTxns.insert(rec.txnId);
      break;
    default:
      break;
    }
  }

  std::cout << "[Recovery] Committed txns: " << committedTxns.size()
            << ", Active (uncommitted): " << activeTxns.size() << std::endl;

  // Collect table names that need file handles
  std::unordered_map<std::string, int> tableFiles;
  for (const auto &rec : records) {
    if (!rec.tableName.empty() && tableFiles.find(rec.tableName) == tableFiles.end()) {
      // Try to open the table file
      int fileId = pageMgr_->openTable(rec.tableName);
      if (fileId >= 0) {
        tableFiles[rec.tableName] = fileId;
      }
    }
  }

  // Redo phase: replay committed transactions
  redoPhase(records, committedTxns, tableFiles);

  // Undo phase: reverse uncommitted transactions
  undoPhase(records, activeTxns, tableFiles);

  std::cout << "[Recovery] Recovery complete." << std::endl;
}

void RecoveryManager::redoPhase(
    const std::vector<LogRecord> &records,
    const std::unordered_set<int> &committedTxns,
    const std::unordered_map<std::string, int> &tableFiles) {

  std::cout << "[Recovery] Redo phase: replaying committed operations..."
            << std::endl;

  for (const auto &rec : records) {
    // Only redo operations from committed transactions
    if (committedTxns.find(rec.txnId) == committedTxns.end()) {
      continue;
    }

    auto fileIt = tableFiles.find(rec.tableName);
    if (fileIt == tableFiles.end()) continue;
    int fileId = fileIt->second;

    switch (rec.type) {
    case LogRecordType::INSERT: {
      // Re-insert the tuple at the logged RID
      // For simplicity, we insert via the PageManager (may get a different RID)
      if (!rec.newData.empty()) {
        pageMgr_->insertTuple(fileId, rec.newData.data(),
                              static_cast<uint16_t>(rec.newData.size()));
        std::cout << "[Recovery] Redo INSERT for txn " << rec.txnId
                  << " in " << rec.tableName << std::endl;
      }
      break;
    }
    case LogRecordType::DELETE: {
      // Re-delete the tuple at the logged RID
      pageMgr_->deleteTuple(fileId, rec.rid);
      std::cout << "[Recovery] Redo DELETE for txn " << rec.txnId
                << " in " << rec.tableName << std::endl;
      break;
    }
    default:
      break;
    }
  }
}

void RecoveryManager::undoPhase(
    const std::vector<LogRecord> &records,
    const std::unordered_set<int> &uncommittedTxns,
    const std::unordered_map<std::string, int> &tableFiles) {

  std::cout << "[Recovery] Undo phase: reversing uncommitted operations..."
            << std::endl;

  // Process records in reverse order to undo
  for (auto it = records.rbegin(); it != records.rend(); ++it) {
    const auto &rec = *it;

    if (uncommittedTxns.find(rec.txnId) == uncommittedTxns.end()) {
      continue;
    }

    auto fileIt = tableFiles.find(rec.tableName);
    if (fileIt == tableFiles.end()) continue;
    int fileId = fileIt->second;

    switch (rec.type) {
    case LogRecordType::INSERT: {
      // Undo insert = delete the tuple
      pageMgr_->deleteTuple(fileId, rec.rid);
      std::cout << "[Recovery] Undo INSERT for txn " << rec.txnId
                << " in " << rec.tableName << std::endl;
      break;
    }
    case LogRecordType::DELETE: {
      // Undo delete = re-insert the old data
      if (!rec.oldData.empty()) {
        pageMgr_->insertTuple(fileId, rec.oldData.data(),
                              static_cast<uint16_t>(rec.oldData.size()));
        std::cout << "[Recovery] Undo DELETE for txn " << rec.txnId
                  << " in " << rec.tableName << std::endl;
      }
      break;
    }
    default:
      break;
    }
  }
}

void RecoveryManager::checkpoint() {
  std::cout << "[Recovery] Writing checkpoint..." << std::endl;

  LogRecord rec;
  rec.type = LogRecordType::CHECKPOINT;
  rec.txnId = 0;
  logMgr_->appendLog(rec);
  logMgr_->flushLog();

  std::cout << "[Recovery] Checkpoint written." << std::endl;
}
