#pragma once
#include "../common/config.h"
#include "../common/rid.h"
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

// Types of log records
enum class LogRecordType {
  INSERT,
  DELETE,
  UPDATE,
  BEGIN_TXN,
  COMMIT_TXN,
  ABORT_TXN,
  CHECKPOINT
};

// A single WAL log record
struct LogRecord {
  uint64_t lsn;           // Log Sequence Number
  LogRecordType type;
  int txnId;              // transaction that made this change
  std::string tableName;  // affected table
  RID rid;                // affected row (for INSERT/DELETE/UPDATE)
  std::vector<char> oldData; // before-image (for undo)
  std::vector<char> newData; // after-image (for redo)

  LogRecord()
      : lsn(0), type(LogRecordType::BEGIN_TXN), txnId(0) {}
};

// LogManager implements Write-Ahead Logging (WAL).
// All modifications are logged before being applied to data pages.
class LogManager {
public:
  LogManager();
  ~LogManager();

  // Open (or create) the WAL file
  void open(const std::string &walPath = "");

  // Append a log record and return its LSN
  uint64_t appendLog(const LogRecord &record);

  // Flush the log buffer to disk (force)
  void flushLog();

  // Read all log records (for recovery)
  std::vector<LogRecord> readAllRecords();

  // Get the current LSN
  uint64_t currentLSN() const { return nextLSN_; }

  // Convenience: log a BEGIN transaction
  uint64_t logBegin(int txnId);

  // Convenience: log a COMMIT
  uint64_t logCommit(int txnId);

  // Convenience: log an ABORT
  uint64_t logAbort(int txnId);

  // Convenience: log an INSERT
  uint64_t logInsert(int txnId, const std::string &tableName,
                     const RID &rid, const std::vector<char> &data);

  // Convenience: log a DELETE
  uint64_t logDelete(int txnId, const std::string &tableName,
                     const RID &rid, const std::vector<char> &oldData);

private:
  std::ofstream logFile_;
  uint64_t nextLSN_;
  std::mutex mutex_;
  std::string walPath_;

  // Serialize a log record to bytes
  std::vector<char> serializeRecord(const LogRecord &record);

  // Deserialize a log record from bytes
  LogRecord deserializeRecord(const char *data, size_t len);
};
