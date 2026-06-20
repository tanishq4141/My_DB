#include "PageManager.h"
#include <iostream>
#include <stdexcept>

static constexpr uint16_t HEADER_SIZE = sizeof(SlottedPageHeader);

PageManager::PageManager(DiskManager *diskMgr, BufferPool *bufferPool)
    : diskMgr_(diskMgr), bufferPool_(bufferPool) {}

int PageManager::createTable(const std::string &tableName) {
  std::string fileName = tableName + ".db";
  int fileId = diskMgr_->createFile(fileName);

  // Allocate the first page and initialize it
  uint32_t pageNum = diskMgr_->allocateNewPage(fileId);
  char *pageData = bufferPool_->fetchPage(fileId, pageNum, true);
  initPage(pageData, pageNum);
  bufferPool_->unpinPage(fileId, pageNum, true);

  return fileId;
}

int PageManager::openTable(const std::string &tableName) {
  std::string fileName = tableName + ".db";
  return diskMgr_->openFile(fileName);
}

void PageManager::initPage(char *pageData, uint32_t pageId) {
  std::memset(pageData, 0, PAGE_SIZE);
  auto *header = reinterpret_cast<SlottedPageHeader *>(pageData);
  header->pageId = pageId;
  header->numSlots = 0;
  header->freeSpacePtr = HEADER_SIZE; // free space starts after header
}

uint32_t PageManager::findFreePage(int fileId, uint16_t requiredSpace) {
  uint32_t pageCount = diskMgr_->getFilePageCount(fileId);

  // Search existing pages for one with enough space
  for (uint32_t p = 0; p < pageCount; p++) {
    char *pageData = bufferPool_->fetchPage(fileId, p, false);
    auto *header = reinterpret_cast<SlottedPageHeader *>(pageData);

    uint16_t usedSpace = header->freeSpacePtr;
    uint16_t freeSpace = PAGE_SIZE - usedSpace;

    bufferPool_->unpinPage(fileId, p, false);

    if (freeSpace >= requiredSpace) {
      return p;
    }
  }

  // No page with enough space — allocate a new one
  uint32_t newPage = diskMgr_->allocateNewPage(fileId);
  char *pageData = bufferPool_->fetchPage(fileId, newPage, true);
  initPage(pageData, newPage);
  bufferPool_->unpinPage(fileId, newPage, true);
  return newPage;
}

RID PageManager::insertTuple(int fileId, const char *data, uint16_t dataLen) {
  // Find a page with enough free space for the tuple
  uint32_t pageNum = findFreePage(fileId, dataLen);

  char *pageData = bufferPool_->fetchPage(fileId, pageNum, true);
  auto *header = reinterpret_cast<SlottedPageHeader *>(pageData);

  // Find an empty slot or use the next slot
  uint16_t slotIdx = header->numSlots;
  for (uint16_t i = 0; i < header->numSlots; i++) {
    if (header->slotDirectory[i] == 0 && header->slotLengths[i] == 0) {
      slotIdx = i;
      break;
    }
  }

  if (slotIdx >= MAX_SLOTS_PER_PAGE) {
    bufferPool_->unpinPage(fileId, pageNum, false);
    // Try allocating a new page
    uint32_t newPage = diskMgr_->allocateNewPage(fileId);
    char *newPageData = bufferPool_->fetchPage(fileId, newPage, true);
    initPage(newPageData, newPage);
    auto *newHeader = reinterpret_cast<SlottedPageHeader *>(newPageData);

    // Insert into the new page
    newHeader->slotDirectory[0] = newHeader->freeSpacePtr;
    newHeader->slotLengths[0] = dataLen;
    std::memcpy(newPageData + newHeader->freeSpacePtr, data, dataLen);
    newHeader->freeSpacePtr += dataLen;
    newHeader->numSlots = 1;

    bufferPool_->unpinPage(fileId, newPage, true);
    return RID(newPage, 0);
  }

  // Write the tuple data at the free space pointer
  header->slotDirectory[slotIdx] = header->freeSpacePtr;
  header->slotLengths[slotIdx] = dataLen;
  std::memcpy(pageData + header->freeSpacePtr, data, dataLen);
  header->freeSpacePtr += dataLen;

  if (slotIdx >= header->numSlots) {
    header->numSlots = slotIdx + 1;
  }

  RID rid(pageNum, slotIdx);
  bufferPool_->unpinPage(fileId, pageNum, true);
  return rid;
}

uint16_t PageManager::readTuple(int fileId, const RID &rid, char *outData) {
  char *pageData = bufferPool_->fetchPage(fileId, rid.pageId, false);
  auto *header = reinterpret_cast<SlottedPageHeader *>(pageData);

  if (rid.slotNum >= header->numSlots ||
      header->slotLengths[rid.slotNum] == 0) {
    bufferPool_->unpinPage(fileId, rid.pageId, false);
    return 0; // slot is empty or invalid
  }

  uint16_t offset = header->slotDirectory[rid.slotNum];
  uint16_t length = header->slotLengths[rid.slotNum];
  std::memcpy(outData, pageData + offset, length);

  bufferPool_->unpinPage(fileId, rid.pageId, false);
  return length;
}

void PageManager::deleteTuple(int fileId, const RID &rid) {
  char *pageData = bufferPool_->fetchPage(fileId, rid.pageId, true);
  auto *header = reinterpret_cast<SlottedPageHeader *>(pageData);

  if (rid.slotNum < header->numSlots) {
    // Mark the slot as deleted (zero out offset and length)
    header->slotDirectory[rid.slotNum] = 0;
    header->slotLengths[rid.slotNum] = 0;
    // Note: we don't compact free space for simplicity
  }

  bufferPool_->unpinPage(fileId, rid.pageId, true);
}

uint32_t PageManager::getPageCount(int fileId) {
  return diskMgr_->getFilePageCount(fileId);
}

void PageManager::scanTable(
    int fileId,
    const std::function<bool(const RID &, const char *, uint16_t)> &callback) {
  uint32_t pageCount = diskMgr_->getFilePageCount(fileId);

  for (uint32_t p = 0; p < pageCount; p++) {
    char *pageData = bufferPool_->fetchPage(fileId, p, false);
    auto *header = reinterpret_cast<SlottedPageHeader *>(pageData);

    for (uint16_t s = 0; s < header->numSlots; s++) {
      if (header->slotLengths[s] > 0) {
        RID rid(p, s);
        const char *tupleData = pageData + header->slotDirectory[s];
        uint16_t tupleLen = header->slotLengths[s];

        if (!callback(rid, tupleData, tupleLen)) {
          bufferPool_->unpinPage(fileId, p, false);
          return;
        }
      }
    }

    bufferPool_->unpinPage(fileId, p, false);
  }
}
