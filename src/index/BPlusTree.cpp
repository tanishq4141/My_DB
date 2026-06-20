#include "BPlusTree.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

BPlusTree::BPlusTree(int order)
    : order_(order), rootIdx_(-1), entryCount_(0) {
  maxLeafKeys_ = order - 1;
  minLeafKeys_ = (order - 1 + 1) / 2; // ceil((order-1)/2)
  maxInternalKeys_ = order - 1;
  minInternalKeys_ = (order + 1) / 2 - 1; // ceil(order/2) - 1
}

int BPlusTree::allocateNode(BNodeType type) {
  BPlusNode node;
  node.type = type;
  node.isRoot = false;
  node.pageId = static_cast<uint32_t>(nodes_.size());
  node.parentIdx = -1;
  node.size = 0;
  node.nextLeafIdx = -1;
  nodes_.push_back(node);
  return static_cast<int>(nodes_.size()) - 1;
}

int BPlusTree::findLeaf(int key) const {
  if (rootIdx_ < 0) return -1;

  int current = rootIdx_;
  while (nodes_[current].type == BNodeType::INTERNAL) {
    const BPlusNode &node = nodes_[current];
    int i = 0;
    while (i < node.size && key >= node.keys[i]) {
      i++;
    }
    current = node.childIndices[i];
  }
  return current;
}

bool BPlusTree::search(int key, RID &result) const {
  int leafIdx = findLeaf(key);
  if (leafIdx < 0) return false;

  const BPlusNode &leaf = nodes_[leafIdx];
  for (int i = 0; i < leaf.size; i++) {
    if (leaf.keys[i] == key) {
      result = leaf.values[i];
      return true;
    }
  }
  return false;
}

bool BPlusTree::insert(int key, const RID &value) {
  // Empty tree: create root leaf
  if (rootIdx_ < 0) {
    rootIdx_ = allocateNode(BNodeType::LEAF);
    nodes_[rootIdx_].isRoot = true;
    nodes_[rootIdx_].keys.push_back(key);
    nodes_[rootIdx_].values.push_back(value);
    nodes_[rootIdx_].size = 1;
    entryCount_++;
    return true;
  }

  // Find the leaf where this key belongs
  int leafIdx = findLeaf(key);

  // Check if key already exists
  BPlusNode &leaf = nodes_[leafIdx];
  for (int i = 0; i < leaf.size; i++) {
    if (leaf.keys[i] == key) {
      return false; // duplicate key
    }
  }

  // If leaf has room, insert directly
  if (leaf.size < maxLeafKeys_) {
    insertIntoLeaf(leafIdx, key, value);
  } else {
    // Leaf is full — need to split
    splitLeafAndInsert(leafIdx, key, value);
  }

  entryCount_++;
  return true;
}

void BPlusTree::insertIntoLeaf(int leafIdx, int key, const RID &value) {
  BPlusNode &leaf = nodes_[leafIdx];

  // Find insertion position (maintain sorted order)
  auto it = std::lower_bound(leaf.keys.begin(), leaf.keys.end(), key);
  int pos = static_cast<int>(it - leaf.keys.begin());

  leaf.keys.insert(leaf.keys.begin() + pos, key);
  leaf.values.insert(leaf.values.begin() + pos, value);
  leaf.size++;
}

void BPlusTree::splitLeafAndInsert(int leafIdx, int key, const RID &value) {
  BPlusNode &leaf = nodes_[leafIdx];

  // Temporarily insert into the leaf
  auto it = std::lower_bound(leaf.keys.begin(), leaf.keys.end(), key);
  int pos = static_cast<int>(it - leaf.keys.begin());
  leaf.keys.insert(leaf.keys.begin() + pos, key);
  leaf.values.insert(leaf.values.begin() + pos, value);
  leaf.size++;

  // Create new leaf node
  int newLeafIdx = allocateNode(BNodeType::LEAF);
  BPlusNode &newLeaf = nodes_[newLeafIdx];

  // Split point: first ceil(n/2) keys stay in left, rest go to right
  int splitPoint = (leaf.size + 1) / 2;

  newLeaf.keys.assign(leaf.keys.begin() + splitPoint, leaf.keys.end());
  newLeaf.values.assign(leaf.values.begin() + splitPoint, leaf.values.end());
  newLeaf.size = static_cast<int>(newLeaf.keys.size());

  leaf.keys.resize(splitPoint);
  leaf.values.resize(splitPoint);
  leaf.size = splitPoint;

  // Link leaves
  newLeaf.nextLeafIdx = leaf.nextLeafIdx;
  leaf.nextLeafIdx = newLeafIdx;

  // The key to promote to parent is the first key of the new leaf
  int promoteKey = newLeaf.keys[0];

  // Set parent for new leaf
  newLeaf.parentIdx = leaf.parentIdx;

  // Insert into parent
  insertIntoParent(leafIdx, promoteKey, newLeafIdx);
}

void BPlusTree::insertIntoParent(int leftIdx, int key, int rightIdx) {
  // If left is root, create a new root
  if (nodes_[leftIdx].isRoot) {
    int newRootIdx = allocateNode(BNodeType::INTERNAL);
    BPlusNode &newRoot = nodes_[newRootIdx];
    newRoot.isRoot = true;
    newRoot.keys.push_back(key);
    newRoot.childIndices.push_back(leftIdx);
    newRoot.childIndices.push_back(rightIdx);
    newRoot.size = 1;

    nodes_[leftIdx].isRoot = false;
    nodes_[leftIdx].parentIdx = newRootIdx;
    nodes_[rightIdx].parentIdx = newRootIdx;

    rootIdx_ = newRootIdx;
    return;
  }

  int parentIdx = nodes_[leftIdx].parentIdx;
  BPlusNode &parent = nodes_[parentIdx];

  if (parent.size < maxInternalKeys_) {
    // Parent has room — insert key and child pointer
    auto it = std::lower_bound(parent.keys.begin(), parent.keys.end(), key);
    int pos = static_cast<int>(it - parent.keys.begin());

    parent.keys.insert(parent.keys.begin() + pos, key);
    parent.childIndices.insert(parent.childIndices.begin() + pos + 1, rightIdx);
    parent.size++;

    nodes_[rightIdx].parentIdx = parentIdx;
  } else {
    // Parent is full — split it
    nodes_[rightIdx].parentIdx = parentIdx;
    splitInternal(parentIdx, key, rightIdx);
  }
}

void BPlusTree::splitInternal(int nodeIdx, int key, int rightChildIdx) {
  BPlusNode &node = nodes_[nodeIdx];

  // Temporarily insert
  auto it = std::lower_bound(node.keys.begin(), node.keys.end(), key);
  int pos = static_cast<int>(it - node.keys.begin());
  node.keys.insert(node.keys.begin() + pos, key);
  node.childIndices.insert(node.childIndices.begin() + pos + 1, rightChildIdx);
  node.size++;

  // Split: the median key is promoted, not kept in either child
  int mid = node.size / 2;
  int promoteKey = node.keys[mid];

  // Create new internal node (right half)
  int newNodeIdx = allocateNode(BNodeType::INTERNAL);
  BPlusNode &newNode = nodes_[newNodeIdx];

  newNode.keys.assign(node.keys.begin() + mid + 1, node.keys.end());
  newNode.childIndices.assign(node.childIndices.begin() + mid + 1,
                              node.childIndices.end());
  newNode.size = static_cast<int>(newNode.keys.size());

  // Update parent pointers for moved children
  for (int childIdx : newNode.childIndices) {
    nodes_[childIdx].parentIdx = newNodeIdx;
  }

  // Trim the left node
  node.keys.resize(mid);
  node.childIndices.resize(mid + 1);
  node.size = mid;

  newNode.parentIdx = node.parentIdx;

  // Promote the median key to the parent
  insertIntoParent(nodeIdx, promoteKey, newNodeIdx);
}

bool BPlusTree::remove(int key) {
  int leafIdx = findLeaf(key);
  if (leafIdx < 0) return false;

  if (!removeFromLeaf(leafIdx, key)) {
    return false;
  }

  entryCount_--;

  // Check for underflow (skip if root)
  if (!nodes_[leafIdx].isRoot && nodes_[leafIdx].size < minLeafKeys_) {
    handleUnderflow(leafIdx);
  }

  // If root is empty internal node, promote its only child
  if (rootIdx_ >= 0 && nodes_[rootIdx_].type == BNodeType::INTERNAL &&
      nodes_[rootIdx_].size == 0) {
    int newRoot = nodes_[rootIdx_].childIndices[0];
    nodes_[newRoot].isRoot = true;
    nodes_[newRoot].parentIdx = -1;
    rootIdx_ = newRoot;
  }

  // If tree is completely empty
  if (rootIdx_ >= 0 && nodes_[rootIdx_].size == 0 &&
      nodes_[rootIdx_].type == BNodeType::LEAF) {
    rootIdx_ = -1;
  }

  return true;
}

bool BPlusTree::removeFromLeaf(int leafIdx, int key) {
  BPlusNode &leaf = nodes_[leafIdx];
  auto it = std::find(leaf.keys.begin(), leaf.keys.end(), key);
  if (it == leaf.keys.end()) return false;

  int pos = static_cast<int>(it - leaf.keys.begin());
  leaf.keys.erase(leaf.keys.begin() + pos);
  leaf.values.erase(leaf.values.begin() + pos);
  leaf.size--;
  return true;
}

void BPlusTree::handleUnderflow(int nodeIdx) {
  BPlusNode &node = nodes_[nodeIdx];
  if (node.isRoot) return;

  int parentIdx = node.parentIdx;
  BPlusNode &parent = nodes_[parentIdx];

  // Find this node's position among parent's children
  int childPos = -1;
  for (size_t i = 0; i < parent.childIndices.size(); i++) {
    if (parent.childIndices[i] == nodeIdx) {
      childPos = static_cast<int>(i);
      break;
    }
  }
  if (childPos < 0) return;

  // Try to borrow from left sibling
  if (childPos > 0) {
    int leftSibIdx = parent.childIndices[childPos - 1];
    BPlusNode &leftSib = nodes_[leftSibIdx];

    if (node.type == BNodeType::LEAF) {
      if (leftSib.size > minLeafKeys_) {
        // Borrow the last key from left sibling
        node.keys.insert(node.keys.begin(), leftSib.keys.back());
        node.values.insert(node.values.begin(), leftSib.values.back());
        leftSib.keys.pop_back();
        leftSib.values.pop_back();
        node.size++;
        leftSib.size--;
        parent.keys[childPos - 1] = node.keys[0];
        return;
      }
    }
  }

  // Try to borrow from right sibling
  if (childPos < static_cast<int>(parent.childIndices.size()) - 1) {
    int rightSibIdx = parent.childIndices[childPos + 1];
    BPlusNode &rightSib = nodes_[rightSibIdx];

    if (node.type == BNodeType::LEAF) {
      if (rightSib.size > minLeafKeys_) {
        // Borrow the first key from right sibling
        node.keys.push_back(rightSib.keys[0]);
        node.values.push_back(rightSib.values[0]);
        rightSib.keys.erase(rightSib.keys.begin());
        rightSib.values.erase(rightSib.values.begin());
        node.size++;
        rightSib.size--;
        parent.keys[childPos] = rightSib.keys[0];
        return;
      }
    }
  }

  // Cannot borrow — merge with a sibling
  if (node.type == BNodeType::LEAF) {
    if (childPos > 0) {
      // Merge with left sibling (append current to left)
      int leftSibIdx = parent.childIndices[childPos - 1];
      BPlusNode &leftSib = nodes_[leftSibIdx];

      for (int i = 0; i < node.size; i++) {
        leftSib.keys.push_back(node.keys[i]);
        leftSib.values.push_back(node.values[i]);
      }
      leftSib.size += node.size;
      leftSib.nextLeafIdx = node.nextLeafIdx;

      // Remove key and child pointer from parent
      parent.keys.erase(parent.keys.begin() + childPos - 1);
      parent.childIndices.erase(parent.childIndices.begin() + childPos);
      parent.size--;
    } else {
      // Merge with right sibling
      int rightSibIdx = parent.childIndices[childPos + 1];
      BPlusNode &rightSib = nodes_[rightSibIdx];

      for (int i = 0; i < rightSib.size; i++) {
        node.keys.push_back(rightSib.keys[i]);
        node.values.push_back(rightSib.values[i]);
      }
      node.size += rightSib.size;
      node.nextLeafIdx = rightSib.nextLeafIdx;

      parent.keys.erase(parent.keys.begin() + childPos);
      parent.childIndices.erase(parent.childIndices.begin() + childPos + 1);
      parent.size--;
    }

    // Check parent underflow
    if (!parent.isRoot && parent.size < minInternalKeys_) {
      handleUnderflow(parentIdx);
    }
  }
}

void BPlusTree::rangeScan(int lowKey, int highKey,
                          std::vector<RID> &results) const {
  int leafIdx = findLeaf(lowKey);
  if (leafIdx < 0) return;

  int current = leafIdx;
  while (current >= 0) {
    const BPlusNode &leaf = nodes_[current];
    for (int i = 0; i < leaf.size; i++) {
      if (leaf.keys[i] >= lowKey && leaf.keys[i] <= highKey) {
        results.push_back(leaf.values[i]);
      }
      if (leaf.keys[i] > highKey) {
        return;
      }
    }
    current = leaf.nextLeafIdx;
  }
}

void BPlusTree::print() const {
  if (rootIdx_ < 0) {
    std::cout << "(empty tree)" << std::endl;
    return;
  }
  printNode(rootIdx_, 0);
}

void BPlusTree::printNode(int nodeIdx, int depth) const {
  const BPlusNode &node = nodes_[nodeIdx];
  std::string indent(depth * 2, ' ');

  if (node.type == BNodeType::LEAF) {
    std::cout << indent << "LEAF [";
    for (int i = 0; i < node.size; i++) {
      std::cout << node.keys[i];
      if (i + 1 < node.size) std::cout << ", ";
    }
    std::cout << "]" << std::endl;
  } else {
    std::cout << indent << "INTERNAL [";
    for (int i = 0; i < node.size; i++) {
      std::cout << node.keys[i];
      if (i + 1 < node.size) std::cout << ", ";
    }
    std::cout << "]" << std::endl;

    for (int childIdx : node.childIndices) {
      printNode(childIdx, depth + 1);
    }
  }
}
