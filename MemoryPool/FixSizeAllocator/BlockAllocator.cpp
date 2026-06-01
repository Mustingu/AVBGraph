//
// Created by Ghufran on 25/01/2023.
//
#include "BlockAllocator.h"

#include <stdlib.h>

#include <iostream>

/* GlobalAllocator function defination */

void GlobalAllocator::init(int fixedLens_, int totalSpace_, bool isInit) {
  fixedLens = fixedLens_;
  if (isInit) {
    totalSpace = totalSpace_ * (1024ul * 1024ul);
    mem = mmap(nullptr, totalSpace, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    // touch each page to insert int to allocate memory
    if (mem == MAP_FAILED && totalSpace == 0) {
      mem = mmap(nullptr, 16 * (1024ul * 1024ul), PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    }
    std::size_t currentAddress1 = 0;

    size_t y;
    for (size_t i = 0; i < totalSpace; i += 0x1000) {
      currentAddress1 = (std::size_t)mem + i;
      size_t *y = reinterpret_cast<size_t *>((void *)currentAddress1);
      *y = 0;
    }

    offset = 0;
  } else {
    totalSpace = totalSpace_ * (1024ul * 1024ul);
    mem = mmap(nullptr, totalSpace, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (mem == MAP_FAILED) {
      std::cout << "error memory" << std::endl;
    }
    // touch each page to insert int to allocate memory
    offset = 0;
  }
}

void *GlobalAllocator::allocate() {
  const std::size_t cur_addr = (std::size_t)mem + offset;
  offset += fixedLens;
  return (void *)cur_addr;
}

unsigned GlobalAllocator::getOffset() { return offset; }

unsigned GlobalAllocator::getTotalSpace() { return totalSpace; }

bool GlobalAllocator::isFreeSpace() {
  if ((offset + fixedLens) > totalSpace) {
    return false;
  } else {
    return true;
  }
}

void FixedSizeVector::newBlockArr(unsigned size) {
  GlobalAllocator *ptr =
      static_cast<GlobalAllocator *>(aligned_alloc(8, sizeof(GlobalAllocator)));
  ptr->init(fixedLens, size, false);
  blockArr[++curIndex] = ptr;
}
/* FixedSizeVector class defination */
void FixedSizeVector::initVal(unsigned fixedLens_) {
  fixedLens = fixedLens_;
  curIndex = -1;
}

// GlobalAllocator **FixedSizeVector::getBlockArr() { return blockArr; }

// int FixedSizeVector::getCurIndex() const { return curIndex; }

bool FixedSizeVector::IsVectorFullorGetBlock(void *&index) {
  if (blockArr[curIndex]->isFreeSpace()) {
    return (index = blockArr[curIndex]->allocate()), false;
  }

  if (curIndex == (MEMORY_VECTOR_SIZE - 1)) {
    return true;
  }

  newBlockArr();
  return (index = blockArr[curIndex]->allocate()), false;
}

void *FixedSizeVector::getBlock() { return blockArr[curIndex]->allocate(); }

void FixedSizeVector::initBlockArray() {
  blockArr = reinterpret_cast<GlobalAllocator **>(
      aligned_alloc(8, 8 * MEMORY_VECTOR_SIZE));

  newBlockArr();
}

/* Two level vector */

void TwoLevelVector::initVal(unsigned fixedLens_) {
  fixedLens = fixedLens_;
  IndrArray = NULL;
  curIndex = -1;
  curSize = 1;
  blockSize = 0;
}

void TwoLevelVector::insert() {
  if (curIndex == (blockSize - 1)) {
    setIndrArray();
  }
  addFSVector();
}

void *TwoLevelVector::getBlock() {
  void *index;
  if (IndrArray[curIndex]->IsVectorFullorGetBlock(index)) {
    insert();
    index = IndrArray[curIndex]->getBlock();
  }
  return index;
}

void TwoLevelVector::addFSVector() {
  FixedSizeVector *new_block =
      static_cast<FixedSizeVector *>(aligned_alloc(8, sizeof(FixedSizeVector)));
  new_block->initVal(fixedLens);
  new_block->initBlockArray();
  IndrArray[++curIndex] = new_block;
}

void TwoLevelVector::setIndrArray() {
  if (IndrArray == NULL) {
    // blockLen = 1;

    IndrArray =
        reinterpret_cast<FixedSizeVector **>(static_cast<FixedSizeVector **>(
            aligned_alloc(8, 8 * (curSize * fixedSize))));
    blockSize = fixedSize;

    addFSVector();

  } else {
    int blenprev = curSize;
    int b_len_new = 2 * curSize;

    FixedSizeVector **temp = static_cast<FixedSizeVector **>(
        aligned_alloc(8, 8 * ((b_len_new)*fixedSize)));

    //  FixedTimeIndexBlock **temp2;
    // todo deletion mechanism ()create node in globle linklist and add the
    // address of deleted index array and with time to replace the garbage
    // collector will deallocate the memory than int k=0; int tempC = (blenprev)
    // * fixedSize;

    int bprev = (blenprev)*fixedSize;

    memcpy(temp, IndrArray, bprev * 8);

    curIndex = blenprev - 1;

    void *ptr = IndrArray;

    IndrArray = temp;
    free(ptr);
    blockSize = b_len_new * fixedSize;

    curSize = b_len_new;
  }
}
