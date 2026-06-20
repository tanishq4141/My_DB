#include "BufferPool.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

BufferPool::BufferPool(DiskManager *diskMgr, uint32_t poolSize)
    : diskMgr_(diskMgr), poolSize_(poolSize), cacheHits_(0), cacheMisses_(0) {
  frames_.resize(poolSize);
  for (uint32_t i = 0; i < poolSize; i++) {
    std::memset(frames_[i].data, 0, PAGE_SIZE);
    frames_[i].pageId = INVALID_PAGE_ID;
    frames_[i].fileId = -1;
    frames_[i].pageNum = 0;
    frames_[i].isDirty = false;
    frames_[i].pinCount = 0;
    freeList_.push_back(i);
  }
}

BufferPool::~BufferPool() { flushAllPages(); }

uint64_t BufferPool::makePageKey(int fileId, uint32_t pageNum) const {
  return (static_cast<uint64_t>(fileId) << 32) | pageNum;
}

char *BufferPool::fetchPage(int fileId, uint32_t pageNum, bool forWrite) {
  std::lock_guard<std::mutex> lock(mutex_);

  uint64_t key = makePageKey(fileId, pageNum);

  // Check if page is already in the pool
  auto it = pageTable_.find(key);
  if (it != pageTable_.end()) {
    cacheHits_++;
    size_t frameIdx = it->second;
    PageFrame &frame = frames_[frameIdx];
    frame.pinCount++;
    if (forWrite) {
      frame.isDirty = true;
    }

    // Remove from LRU list if it was there (now it's pinned again)
    auto lruIt = lruMap_.find(frameIdx);
    if (lruIt != lruMap_.end()) {
      lruList_.erase(lruIt->second);
      lruMap_.erase(lruIt);
    }

    return frame.data;
  }

  // Cache miss: need to load from disk
  cacheMisses_++;
  size_t frameIdx = findVictimFrame();

  PageFrame &frame = frames_[frameIdx];

  // If this frame held a different page, remove it from page table
  if (frame.fileId >= 0 && frame.pageId != INVALID_PAGE_ID) {
    uint64_t oldKey = makePageKey(frame.fileId, frame.pageNum);
    pageTable_.erase(oldKey);

    // Write back dirty page before evicting
    if (frame.isDirty) {
      diskMgr_->writePage(frame.fileId, frame.pageNum, frame.data);
    }
  }

  // Load the requested page from disk
  diskMgr_->readPage(fileId, pageNum, frame.data);
  frame.fileId = fileId;
  frame.pageNum = pageNum;
  frame.pageId = pageNum;
  frame.isDirty = forWrite;
  frame.pinCount = 1;

  pageTable_[key] = frameIdx;
  return frame.data;
}

void BufferPool::unpinPage(int fileId, uint32_t pageNum, bool isDirty) {
  std::lock_guard<std::mutex> lock(mutex_);

  uint64_t key = makePageKey(fileId, pageNum);
  auto it = pageTable_.find(key);
  if (it == pageTable_.end()) return;

  size_t frameIdx = it->second;
  PageFrame &frame = frames_[frameIdx];

  if (frame.pinCount <= 0) return;
  frame.pinCount--;

  if (isDirty) {
    frame.isDirty = true;
  }

  // If pin count reaches 0, add to LRU list (eligible for eviction)
  if (frame.pinCount == 0) {
    lruList_.push_front(frameIdx);
    lruMap_[frameIdx] = lruList_.begin();
  }
}

void BufferPool::flushPage(int fileId, uint32_t pageNum) {
  std::lock_guard<std::mutex> lock(mutex_);

  uint64_t key = makePageKey(fileId, pageNum);
  auto it = pageTable_.find(key);
  if (it == pageTable_.end()) return;

  PageFrame &frame = frames_[it->second];
  if (frame.isDirty) {
    diskMgr_->writePage(fileId, pageNum, frame.data);
    frame.isDirty = false;
  }
}

void BufferPool::flushAllPages() {
  std::lock_guard<std::mutex> lock(mutex_);

  for (auto &[key, frameIdx] : pageTable_) {
    PageFrame &frame = frames_[frameIdx];
    if (frame.isDirty && frame.fileId >= 0) {
      diskMgr_->writePage(frame.fileId, frame.pageNum, frame.data);
      frame.isDirty = false;
    }
  }
}

size_t BufferPool::findVictimFrame() {
  // First check free list
  if (!freeList_.empty()) {
    size_t idx = freeList_.front();
    freeList_.pop_front();
    return idx;
  }

  // Evict from LRU (back = least recently used)
  if (lruList_.empty()) {
    throw std::runtime_error(
        "BufferPool: all frames are pinned, cannot evict");
  }

  size_t victimIdx = lruList_.back();
  lruList_.pop_back();
  lruMap_.erase(victimIdx);

  return victimIdx;
}
