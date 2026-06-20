#pragma once
#include "../common/config.h"
#include "../common/rid.h"
#include "../common/tuple.h"
#include "BufferPool.h"
#include "DiskManager.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// Slotted page header stored at the beginning of each heap page
struct SlottedPageHeader {
  uint32_t pageId;       // this page's ID
  uint16_t numSlots;     // number of slots used
  uint16_t freeSpacePtr; // offset to start of free space
  uint16_t slotDirectory[MAX_SLOTS_PER_PAGE]; // offset of each tuple (0 = empty)
  uint16_t slotLengths[MAX_SLOTS_PER_PAGE];   // length of each tuple
};

// PageManager manages heap files for tables.
// Each table is stored as a sequence of slotted pages.
class PageManager {
public:
  PageManager(DiskManager *diskMgr, BufferPool *bufferPool);

  // Create a new heap file for a table, returns file ID
  int createTable(const std::string &tableName);

  // Open an existing table's heap file
  int openTable(const std::string &tableName);

  // Insert a serialized tuple into a table, returns the RID where it was placed
  RID insertTuple(int fileId, const char *data, uint16_t dataLen);

  // Read a tuple from a specific RID. Returns the data length.
  // Caller provides a buffer of at least PAGE_SIZE bytes.
  uint16_t readTuple(int fileId, const RID &rid, char *outData);

  // Delete a tuple at the given RID
  void deleteTuple(int fileId, const RID &rid);

  // Get total number of pages in a table file
  uint32_t getPageCount(int fileId);

  // Iterate over all valid tuples in a table.
  // Callback receives (RID, data pointer, data length). Return false to stop.
  void scanTable(int fileId,
                 const std::function<bool(const RID &, const char *, uint16_t)> &callback);

private:
  DiskManager *diskMgr_;
  BufferPool *bufferPool_;

  // Initialize a new page with the slotted page header
  void initPage(char *pageData, uint32_t pageId);

  // Find a page with enough free space, or allocate a new one
  uint32_t findFreePage(int fileId, uint16_t requiredSpace);
};
