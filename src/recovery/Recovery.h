#pragma once
#include "../storage/PageManager.h"
#include "LogManager.h"
#include <string>
#include <unordered_map>
#include <unordered_set>

// Recovery Manager implements crash recovery using WAL.
// Uses a simplified ARIES-style approach:
//   1. Read the log from the beginning (or last checkpoint)
//   2. Redo all committed transaction operations
//   3. Undo all uncommitted transaction operations
class RecoveryManager {
public:
  RecoveryManager(LogManager *logMgr, PageManager *pageMgr);

  // Perform crash recovery: read log, redo committed, undo uncommitted
  void recover();

  // Create a checkpoint (flush all dirty pages and write checkpoint record)
  void checkpoint();

private:
  LogManager *logMgr_;
  PageManager *pageMgr_;

  // Redo phase: replay all logged operations for committed transactions
  void redoPhase(const std::vector<LogRecord> &records,
                 const std::unordered_set<int> &committedTxns,
                 const std::unordered_map<std::string, int> &tableFiles);

  // Undo phase: reverse operations of uncommitted transactions
  void undoPhase(const std::vector<LogRecord> &records,
                 const std::unordered_set<int> &uncommittedTxns,
                 const std::unordered_map<std::string, int> &tableFiles);
};
