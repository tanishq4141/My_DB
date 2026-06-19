#pragma once
#include <cstdint>

// ============================================================
// Global configuration constants for MiniDB
// ============================================================

// Page size in bytes (4 KB)
static constexpr uint32_t PAGE_SIZE = 4096;

// Maximum number of pages the buffer pool can hold
static constexpr uint32_t BUFFER_POOL_SIZE = 1024;

// Maximum number of columns per table
static constexpr uint32_t MAX_COLUMNS = 64;

// Maximum length of a TEXT value in bytes
static constexpr uint32_t MAX_VARCHAR_LEN = 255;

// Maximum number of slots per page (for slotted page format)
static constexpr uint32_t MAX_SLOTS_PER_PAGE = 128;

// B+ Tree order (max children per internal node)
static constexpr uint32_t BPLUS_TREE_ORDER = 128;

// B+ Tree leaf max entries
static constexpr uint32_t BPLUS_TREE_LEAF_MAX = 127;

// Invalid page ID sentinel
static constexpr uint32_t INVALID_PAGE_ID = 0xFFFFFFFF;

// WAL log file name
static constexpr const char *WAL_FILE_NAME = "minidb.wal";

// Data directory for table files
static constexpr const char *DATA_DIR = "minidb_data";
