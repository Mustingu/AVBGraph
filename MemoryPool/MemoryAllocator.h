// MemoryAllocator.h

#ifndef MEMORY_ALLOCATOR_H
#define MEMORY_ALLOCATOR_H

#include "BlockAllocator.h"
#include "FreeListQueue.h"

struct AllocatorBlock {
  bool isAvailable;
  TwoLevelVector *memBlock;
  TwoLevelQueue *freeList;
};

struct PerThreadMemoryAllocator {
 public:
  PerThreadMemoryAllocator();

  void init(unsigned fixedLens_, int space, int total_thread);

  void *Allocate();

  // void* getBlock(int index, int requestedBlock);
  void free(void *ptr);

  int curIndex;
  int totalThread;
  int spacePerThread;
  int thread_id;

  unsigned fixedLens;
  // own two level vector implementation
  AllocatorBlock block;
};

class MemoryAllocator {
 public:
  void init(unsigned fixedLens_, int totalSpace, int totalThread);

  void *Allocate(int thread_id);

  void free(void *ptr, int thread_id);

  PerThreadMemoryAllocator *getPerThreadMemoryAllocator(int thread_id);

  PerThreadMemoryAllocator **perThreadArr;
  unsigned fixedLens;
};

#endif  // MEMORY_ALLOCATOR_H