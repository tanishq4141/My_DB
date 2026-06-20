#pragma once
#include "../common/config.h"
#include "DiskManager.h"
#include <cstdint>
#include <list>
#include <mutex>
#include <unordered_map>

// A single page frame in the buffer pool
struct PageFrame {
  char data[PAGE_SIZE]; // raw page data
  uint32_t pageId;      // which page this frame holds (global: fileId<<16 | pageNum)
  int fileId;           // file this page belongs to
  uint32_t pageNum;     // page number within the file
  bool isDirty;         // has this page been modified?
  int pinCount;         // how many users currently reference this page
};

// BufferPool: fixed-size in-memory cache of disk pages.
// Uses LRU eviction policy. Pages must be pinned before access
// and unpinned after use.
class BufferPool {
public:
  BufferPool(DiskManager *diskMgr, uint32_t poolSize = BUFFER_POOL_SIZE);
  ~BufferPool();

  // Fetch a page into the buffer pool. If forWrite=true, marks it dirty.
  // Returns pointer to the page data. Increments pin count.
  char *fetchPage(int fileId, uint32_t pageNum, bool forWrite = false);

  // Unpin a page. Optionally mark dirty.
  void unpinPage(int fileId, uint32_t pageNum, bool isDirty = false);

  // Flush a specific page to disk
  void flushPage(int fileId, uint32_t pageNum);

  // Flush all dirty pages to disk
  void flushAllPages();

  // Get total cache hits/misses for monitoring
  uint64_t getCacheHits() const { return cacheHits_; }
  uint64_t getCacheMisses() const { return cacheMisses_; }

private:
  DiskManager *diskMgr_;
  uint32_t poolSize_;

  // Pool of page frames
  std::vector<PageFrame> frames_;

  // Map from (global page ID) -> frame index
  std::unordered_map<uint64_t, size_t> pageTable_;

  // LRU list: front = most recently used, back = least recently used
  // Stores frame indices. Only unpinned pages are in this list.
  std::list<size_t> lruList_;
  std::unordered_map<size_t, std::list<size_t>::iterator> lruMap_;

  // Free frame list
  std::list<size_t> freeList_;

  std::mutex mutex_;
  uint64_t cacheHits_;
  uint64_t cacheMisses_;

  // Create a global page key from fileId and pageNum
  uint64_t makePageKey(int fileId, uint32_t pageNum) const;

  // Find a free frame or evict one using LRU
  size_t findVictimFrame();
};
