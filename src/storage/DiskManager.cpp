#include "DiskManager.h"
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

DiskManager::DiskManager() : nextFileId_(1) { ensureDataDir(); }

DiskManager::~DiskManager() {
  for (auto &[id, handle] : files_) {
    if (handle.stream.is_open()) {
      handle.stream.close();
    }
  }
}

void DiskManager::ensureDataDir() {
  if (!fs::exists(DATA_DIR)) {
    fs::create_directories(DATA_DIR);
  }
}

int DiskManager::createFile(const std::string &fileName) {
  std::lock_guard<std::mutex> lock(mutex_);

  std::string fullPath = std::string(DATA_DIR) + "/" + fileName;

  // Create the file (truncate if exists)
  std::fstream fs(fullPath,
                  std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
  if (!fs.is_open()) {
    // Try creating it fresh
    std::ofstream create(fullPath, std::ios::binary);
    create.close();
    fs.open(fullPath, std::ios::in | std::ios::out | std::ios::binary);
  }

  if (!fs.is_open()) {
    throw std::runtime_error("DiskManager: cannot create file: " + fullPath);
  }

  int fileId = nextFileId_++;
  files_[fileId] = {std::move(fs), fullPath, 0};
  return fileId;
}

int DiskManager::openFile(const std::string &fileName) {
  std::lock_guard<std::mutex> lock(mutex_);

  std::string fullPath = std::string(DATA_DIR) + "/" + fileName;
  std::fstream fs(fullPath, std::ios::in | std::ios::out | std::ios::binary);
  if (!fs.is_open()) {
    return -1;
  }

  // Determine page count from file size
  fs.seekg(0, std::ios::end);
  auto fileSize = fs.tellg();
  uint32_t pageCount = static_cast<uint32_t>(fileSize) / PAGE_SIZE;
  fs.seekg(0, std::ios::beg);

  int fileId = nextFileId_++;
  files_[fileId] = {std::move(fs), fullPath, pageCount};
  return fileId;
}

void DiskManager::closeFile(int fileId) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = files_.find(fileId);
  if (it != files_.end()) {
    it->second.stream.close();
    files_.erase(it);
  }
}

void DiskManager::readPage(int fileId, uint32_t pageNum, char *buffer) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = files_.find(fileId);
  if (it == files_.end()) {
    throw std::runtime_error("DiskManager: invalid file ID");
  }

  auto &handle = it->second;
  uint64_t offset = static_cast<uint64_t>(pageNum) * PAGE_SIZE;
  handle.stream.seekg(offset);

  if (!handle.stream.good()) {
    // Page beyond file — return zeroed page
    std::memset(buffer, 0, PAGE_SIZE);
    return;
  }

  handle.stream.read(buffer, PAGE_SIZE);

  // If we read less than a full page (e.g. end of file), zero the rest
  auto bytesRead = handle.stream.gcount();
  if (bytesRead < PAGE_SIZE) {
    std::memset(buffer + bytesRead, 0, PAGE_SIZE - bytesRead);
  }
  handle.stream.clear(); // clear any EOF flags
}

void DiskManager::writePage(int fileId, uint32_t pageNum, const char *buffer) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = files_.find(fileId);
  if (it == files_.end()) {
    throw std::runtime_error("DiskManager: invalid file ID");
  }

  auto &handle = it->second;
  uint64_t offset = static_cast<uint64_t>(pageNum) * PAGE_SIZE;
  handle.stream.seekp(offset);
  handle.stream.write(buffer, PAGE_SIZE);
  handle.stream.flush();

  if (pageNum >= handle.pageCount) {
    handle.pageCount = pageNum + 1;
  }
}

uint32_t DiskManager::getFilePageCount(int fileId) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = files_.find(fileId);
  if (it == files_.end()) return 0;
  return it->second.pageCount;
}

uint32_t DiskManager::allocateNewPage(int fileId) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = files_.find(fileId);
  if (it == files_.end()) {
    throw std::runtime_error("DiskManager: invalid file ID");
  }

  uint32_t newPageNum = it->second.pageCount;
  it->second.pageCount++;

  // Write a zeroed page to extend the file
  char zeroed[PAGE_SIZE];
  std::memset(zeroed, 0, PAGE_SIZE);
  uint64_t offset = static_cast<uint64_t>(newPageNum) * PAGE_SIZE;
  it->second.stream.seekp(offset);
  it->second.stream.write(zeroed, PAGE_SIZE);
  it->second.stream.flush();

  return newPageNum;
}

void DiskManager::flushFile(int fileId) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = files_.find(fileId);
  if (it != files_.end()) {
    it->second.stream.flush();
  }
}
