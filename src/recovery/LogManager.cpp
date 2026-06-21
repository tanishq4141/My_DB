#include "LogManager.h"
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

LogManager::LogManager() : nextLSN_(1) {}

LogManager::~LogManager() {
  if (logFile_.is_open()) {
    logFile_.flush();
    logFile_.close();
  }
}

void LogManager::open(const std::string &walPath) {
  std::lock_guard<std::mutex> lock(mutex_);

  walPath_ = walPath.empty() ? std::string(DATA_DIR) + "/" + WAL_FILE_NAME : walPath;

  // Ensure data directory exists
  if (!fs::exists(DATA_DIR)) {
    fs::create_directories(DATA_DIR);
  }

  // Check if WAL file exists and has records (to set LSN)
  if (fs::exists(walPath_)) {
    std::ifstream reader(walPath_, std::ios::binary | std::ios::ate);
    if (reader.is_open()) {
      auto fileSize = reader.tellg();
      if (fileSize > 0) {
        // Read to find max LSN
        auto records = readAllRecords();
        for (const auto &rec : records) {
          if (rec.lsn >= nextLSN_) {
            nextLSN_ = rec.lsn + 1;
          }
        }
      }
    }
  }

  logFile_.open(walPath_, std::ios::binary | std::ios::app);
  if (!logFile_.is_open()) {
    throw std::runtime_error("LogManager: cannot open WAL file: " + walPath_);
  }
}

std::vector<char> LogManager::serializeRecord(const LogRecord &record) {
  std::vector<char> buffer;

  // LSN (8 bytes)
  const char *lsnBytes = reinterpret_cast<const char *>(&record.lsn);
  buffer.insert(buffer.end(), lsnBytes, lsnBytes + sizeof(uint64_t));

  // Type (1 byte)
  uint8_t type = static_cast<uint8_t>(record.type);
  buffer.push_back(static_cast<char>(type));

  // TxnId (4 bytes)
  const char *txnBytes = reinterpret_cast<const char *>(&record.txnId);
  buffer.insert(buffer.end(), txnBytes, txnBytes + sizeof(int));

  // Table name (2 bytes length + string)
  uint16_t nameLen = static_cast<uint16_t>(record.tableName.size());
  const char *nameLenBytes = reinterpret_cast<const char *>(&nameLen);
  buffer.insert(buffer.end(), nameLenBytes, nameLenBytes + sizeof(uint16_t));
  buffer.insert(buffer.end(), record.tableName.begin(), record.tableName.end());

  // RID (6 bytes: 4 pageId + 2 slotNum)
  const char *ridPageBytes = reinterpret_cast<const char *>(&record.rid.pageId);
  buffer.insert(buffer.end(), ridPageBytes, ridPageBytes + sizeof(uint32_t));
  const char *ridSlotBytes = reinterpret_cast<const char *>(&record.rid.slotNum);
  buffer.insert(buffer.end(), ridSlotBytes, ridSlotBytes + sizeof(uint16_t));

  // Old data (4 bytes length + data)
  uint32_t oldLen = static_cast<uint32_t>(record.oldData.size());
  const char *oldLenBytes = reinterpret_cast<const char *>(&oldLen);
  buffer.insert(buffer.end(), oldLenBytes, oldLenBytes + sizeof(uint32_t));
  buffer.insert(buffer.end(), record.oldData.begin(), record.oldData.end());

  // New data (4 bytes length + data)
  uint32_t newLen = static_cast<uint32_t>(record.newData.size());
  const char *newLenBytes = reinterpret_cast<const char *>(&newLen);
  buffer.insert(buffer.end(), newLenBytes, newLenBytes + sizeof(uint32_t));
  buffer.insert(buffer.end(), record.newData.begin(), record.newData.end());

  return buffer;
}

LogRecord LogManager::deserializeRecord(const char *data, size_t len) {
  LogRecord record;
  size_t offset = 0;

  // LSN
  std::memcpy(&record.lsn, data + offset, sizeof(uint64_t));
  offset += sizeof(uint64_t);

  // Type
  record.type = static_cast<LogRecordType>(static_cast<uint8_t>(data[offset]));
  offset++;

  // TxnId
  std::memcpy(&record.txnId, data + offset, sizeof(int));
  offset += sizeof(int);

  // Table name
  uint16_t nameLen;
  std::memcpy(&nameLen, data + offset, sizeof(uint16_t));
  offset += sizeof(uint16_t);
  record.tableName = std::string(data + offset, nameLen);
  offset += nameLen;

  // RID
  std::memcpy(&record.rid.pageId, data + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  std::memcpy(&record.rid.slotNum, data + offset, sizeof(uint16_t));
  offset += sizeof(uint16_t);

  // Old data
  uint32_t oldLen;
  std::memcpy(&oldLen, data + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  record.oldData.assign(data + offset, data + offset + oldLen);
  offset += oldLen;

  // New data
  uint32_t newLen;
  std::memcpy(&newLen, data + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  record.newData.assign(data + offset, data + offset + newLen);
  offset += newLen;

  return record;
}

uint64_t LogManager::appendLog(const LogRecord &record) {
  std::lock_guard<std::mutex> lock(mutex_);

  LogRecord rec = record;
  rec.lsn = nextLSN_++;

  std::vector<char> serialized = serializeRecord(rec);

  // Write record size (4 bytes) followed by the record
  uint32_t recordSize = static_cast<uint32_t>(serialized.size());
  logFile_.write(reinterpret_cast<const char *>(&recordSize), sizeof(uint32_t));
  logFile_.write(serialized.data(), serialized.size());

  return rec.lsn;
}

void LogManager::flushLog() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (logFile_.is_open()) {
    logFile_.flush();
  }
}

std::vector<LogRecord> LogManager::readAllRecords() {
  std::vector<LogRecord> records;

  std::ifstream reader(walPath_, std::ios::binary);
  if (!reader.is_open()) return records;

  while (reader.good() && !reader.eof()) {
    uint32_t recordSize;
    reader.read(reinterpret_cast<char *>(&recordSize), sizeof(uint32_t));

    if (!reader.good() || reader.gcount() < static_cast<std::streamsize>(sizeof(uint32_t))) {
      break;
    }

    std::vector<char> buffer(recordSize);
    reader.read(buffer.data(), recordSize);

    if (reader.gcount() < static_cast<std::streamsize>(recordSize)) {
      break; // incomplete record
    }

    records.push_back(deserializeRecord(buffer.data(), recordSize));
  }

  return records;
}

uint64_t LogManager::logBegin(int txnId) {
  LogRecord rec;
  rec.type = LogRecordType::BEGIN_TXN;
  rec.txnId = txnId;
  return appendLog(rec);
}

uint64_t LogManager::logCommit(int txnId) {
  LogRecord rec;
  rec.type = LogRecordType::COMMIT_TXN;
  rec.txnId = txnId;
  uint64_t lsn = appendLog(rec);
  flushLog(); // force flush on commit for durability
  return lsn;
}

uint64_t LogManager::logAbort(int txnId) {
  LogRecord rec;
  rec.type = LogRecordType::ABORT_TXN;
  rec.txnId = txnId;
  return appendLog(rec);
}

uint64_t LogManager::logInsert(int txnId, const std::string &tableName,
                               const RID &rid, const std::vector<char> &data) {
  LogRecord rec;
  rec.type = LogRecordType::INSERT;
  rec.txnId = txnId;
  rec.tableName = tableName;
  rec.rid = rid;
  rec.newData = data;
  return appendLog(rec);
}

uint64_t LogManager::logDelete(int txnId, const std::string &tableName,
                               const RID &rid,
                               const std::vector<char> &oldData) {
  LogRecord rec;
  rec.type = LogRecordType::DELETE;
  rec.txnId = txnId;
  rec.tableName = tableName;
  rec.rid = rid;
  rec.oldData = oldData;
  return appendLog(rec);
}
