#ifndef LINEARALLOCATOR_H
#define LINEARALLOCATOR_H

#include <sys/mman.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <unordered_map>
#include <utility>  //declarations of unique_ptr
#include <vector>

#define MEMORY_VECTOR_SIZE 16
#define FOURMB 4L * (1024L) * (1024L)
#define EIGHTMB 8L * (1024L) * (1024L)
#define SIXTEENMB 16L * (1024L) * (1024L)
#define ONEGB (1024L) * (1024L) * (1024L)

#define fixedSize 1

class TwoLevelVector;

class FixedSizeVector;

class PerThreadMemoryAllocator;

class HashTableMemoryIndex;

class GlobalAllocator {
 public:
  void init(int fixedLens_, int totalSpace_, bool isInit);

  void *allocate();

  unsigned getOffset();

  unsigned getTotalSpace();

  bool isFreeSpace();

 private:
  void *mem;
  unsigned offset;
  unsigned totalSpace;
  unsigned fixedLens;
  // todo mutex lock on allocation
};

// fixed size vector

class FixedSizeVector {
 public:
  void initVal(unsigned fixedLens_);

  void newBlockArr(unsigned size = 4);

  void initBlockArray();

  // GlobalAllocator **getBlockArr();

  // int getCurIndex() const;

  // bool IscurIndexFull() const;

  void *getBlock();

  bool IsVectorFullorGetBlock(void *&index);

 private:
  GlobalAllocator **blockArr;
  int curIndex;
  unsigned fixedLens;
};

// Two level vector implementation
class TwoLevelVector {
 public:
  void initVal(unsigned fixedLens_);

  void insert();

  void addFSVector();

  void *getBlock();

  void setIndrArray();

  int curIndex;
  int curSize;
  int blockSize;

  unsigned fixedLens;

  FixedSizeVector **IndrArray;
};

#endif /* LINEARALLOCATOR_H */