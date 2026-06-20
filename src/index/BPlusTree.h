#pragma once
#include "../common/rid.h"
#include <cstdint>
#include <iostream>
#include <vector>

// B+ Tree node types
enum class BNodeType { INTERNAL, LEAF };

// A B+ Tree node (fits in memory for simplicity in this implementation)
struct BPlusNode {
  BNodeType type;
  bool isRoot;
  uint32_t pageId;  // associated page ID (for disk-backed trees)
  int parentIdx;    // index into the node array for the parent (-1 if root)
  int size;         // number of keys currently stored

  std::vector<int> keys;           // sorted keys
  std::vector<RID> values;         // leaf: RID values corresponding to keys
  std::vector<int> childIndices;   // internal: indices into node array for children
  int nextLeafIdx;                 // leaf: index of the next leaf node (-1 if last)

  BPlusNode()
      : type(BNodeType::LEAF), isRoot(false), pageId(0), parentIdx(-1),
        size(0), nextLeafIdx(-1) {}
};

// B+ Tree index on integer keys mapping to RIDs.
// Supports search, insert, delete, and range scan.
class BPlusTree {
public:
  // Create a B+Tree with the given order (max children per internal node)
  explicit BPlusTree(int order = 128);

  // Search for a key. Returns true if found and sets the result RID.
  bool search(int key, RID &result) const;

  // Insert a (key, RID) entry. Returns false if key already exists.
  bool insert(int key, const RID &value);

  // Remove a key. Returns false if key not found.
  bool remove(int key);

  // Range scan: find all keys in [lowKey, highKey] and append their RIDs
  void rangeScan(int lowKey, int highKey, std::vector<RID> &results) const;

  // Print the tree structure (for debugging)
  void print() const;

  // Get number of entries in the tree
  int getSize() const { return entryCount_; }

  bool isEmpty() const { return nodes_.empty(); }

private:
  int order_;            // max children per internal node
  int maxLeafKeys_;      // max keys in a leaf = order - 1
  int minLeafKeys_;      // min keys in a leaf = ceil((order-1)/2)
  int maxInternalKeys_;  // max keys in internal = order - 1
  int minInternalKeys_;  // min keys in internal = ceil(order/2) - 1

  std::vector<BPlusNode> nodes_;  // all nodes stored in a vector
  int rootIdx_;                   // index of the root node
  int entryCount_;                // total number of entries

  // Allocate a new node, returns its index
  int allocateNode(BNodeType type);

  // Find the leaf node that should contain the given key
  int findLeaf(int key) const;

  // Insert into a leaf that is not full
  void insertIntoLeaf(int leafIdx, int key, const RID &value);

  // Split a full leaf and insert
  void splitLeafAndInsert(int leafIdx, int key, const RID &value);

  // Insert a key into an internal node after a child split
  void insertIntoParent(int leftIdx, int key, int rightIdx);

  // Split an internal node
  void splitInternal(int nodeIdx, int key, int rightChildIdx);

  // Remove from leaf
  bool removeFromLeaf(int leafIdx, int key);

  // Handle underflow after deletion
  void handleUnderflow(int nodeIdx);

  // Print helper
  void printNode(int nodeIdx, int depth) const;
};
