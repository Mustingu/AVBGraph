// FreeListQueue.h
#ifndef FREELISTQUEUE_H
#define FREELISTQUEUE_H

#include <string.h>

#include <cstdlib>
#include <vector>

#define MEMORY_VECTOR_SIZE 16
#define fixedSize_queue 16
#define MEMORY_QUEUE_SIZE 1024

class FixedSizeBlock {
 public:
  void initVal();

  void push(void *val);

  void *pop();

  void initBlockArr();

  void **getBlockArr();

  int getCurIndex() const;

  bool IscurIndexFull() const;

  void **blockArr;
  int cur;
};

// Two level vector implementation
class TwoLevelQueue {
 public:
  void initVal();

  void insert(void *ga);

  void *get_block();

  bool empty();

  bool emptyMultipleBlock(int required_block);

  void addBlock();

  void setIndrArray();

  FixedSizeBlock **IndrArray;
  int curIndex;
  int curSize;
  int blockSize;
};

#endif  // FREELISTQUEUE_H