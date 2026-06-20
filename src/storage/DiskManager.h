#pragma once
#include "../common/config.h"
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>

// DiskManager handles raw file I/O for MiniDB.
// Each table gets its own file. Pages are read/written at fixed offsets.
class DiskManager {
public:
  DiskManager();
  ~DiskManager();

  // Create a new file for a table, returns a file ID
  int createFile(const std::string &fileName);

  // Open an existing file, returns file ID (or -1 on failure)
  int openFile(const std::string &fileName);

  // Close a file by ID
  void closeFile(int fileId);

  // Read a raw page from disk into the provided buffer
  void readPage(int fileId, uint32_t pageNum, char *buffer);

  // Write a raw page from the buffer to disk
  void writePage(int fileId, uint32_t pageNum, const char *buffer);

  // Get the number of pages currently in a file
  uint32_t getFilePageCount(int fileId);

  // Allocate a new page at the end of the file, returns the new page number
  uint32_t allocateNewPage(int fileId);

  // Flush a specific file to ensure data is persisted
  void flushFile(int fileId);

private:
  struct FileHandle {
    std::fstream stream;
    std::string fileName;
    uint32_t pageCount;
  };

  std::unordered_map<int, FileHandle> files_;
  int nextFileId_;
  std::mutex mutex_;

  // Ensure the data directory exists
  void ensureDataDir();
};
