
#include "FreeListQueue.h"

void FixedSizeBlock::initVal() { cur = -1; }

void FixedSizeBlock::initBlockArr() {
  blockArr = reinterpret_cast<void **>(aligned_alloc(8, 8 * MEMORY_QUEUE_SIZE));
}

void FixedSizeBlock::push(void *val) { blockArr[++cur] = val; }

void *FixedSizeBlock::pop() {
  void *pt = blockArr[cur];
  cur = cur - 1;
  return pt;
}

void **FixedSizeBlock::getBlockArr() { return blockArr; }

int FixedSizeBlock::getCurIndex() const { return cur; }

bool FixedSizeBlock::IscurIndexFull() const {
  if (cur == (MEMORY_QUEUE_SIZE - 1)) {
    return true;
  } else {
    return false;
  }
}

/* TwoLevelQueue class defination */
void TwoLevelQueue::initVal() {
  curIndex = -1;
  curSize = 1;
  blockSize = 0;
  IndrArray = NULL;
}

//  template<class T>
void TwoLevelQueue::insert(void *ga) {
  if (curIndex == (blockSize - 1)) {
    if (IndrArray[curIndex]->IscurIndexFull()) {
      setIndrArray();
      addBlock();
      IndrArray[curIndex]->push(ga);
    } else {
      IndrArray[curIndex]->push(ga);
    }
  } else {
    // start from here
    if (IndrArray[curIndex]->IscurIndexFull()) {
      addBlock();
      IndrArray[curIndex]->push(ga);
    } else {
      IndrArray[curIndex]->push(ga);
    }
  }
}

void *TwoLevelQueue::get_block() {
  if (curIndex == 0) {
    return IndrArray[curIndex]->pop();
  } else {
    if (IndrArray[curIndex]->getCurIndex() == -1) {
      curIndex = curIndex - 1;
      return IndrArray[curIndex]->pop();
    } else {
      return IndrArray[curIndex]->pop();
    }
  }
}

bool TwoLevelQueue::empty() {
  if (curIndex == 0 && IndrArray[curIndex]->getCurIndex() == -1) {
    return true;
  } else {
    return false;
  }
}

void TwoLevelQueue::addBlock() {
  FixedSizeBlock *new_block =
      static_cast<FixedSizeBlock *>(aligned_alloc(8, sizeof(FixedSizeBlock)));
  new_block->initVal();
  new_block->initBlockArr();
  IndrArray[++curIndex] = new_block;
}

void TwoLevelQueue::setIndrArray() {
  if (IndrArray == NULL) {
    // blockLen = 1;

    IndrArray =
        reinterpret_cast<FixedSizeBlock **>(static_cast<FixedSizeBlock **>(
            aligned_alloc(8, (8 * (curSize * fixedSize_queue)))));
    ;  // reinterpret_cast<FixedSizeBlock **>(static_cast<FixedSizeBlock
       // **>(std::aligned_alloc(8,( 8 *(curSize *
       // fixedSize_queue)))));//reinterpret_cast<FixedSizeBlock
       // **>(static_cast<FixedSizeBlock **>(ptr->Allocate(( 8 *(curSize *
       // fixedSize_queue)))));;//reinterpret_cast<FixedSizeBlock
       // **>(static_cast<FixedSizeBlock **>(std::aligned_alloc(8,( 8 *(curSize
       // * fixedSize_queue)))));//static_cast<EdgeBlock **>(malloc(8 *
       // (blockLen * fixedSize)));

    memset(IndrArray, 0, (curSize * fixedSize_queue));
    blockSize = fixedSize_queue;
    FixedSizeBlock *new_block =
        static_cast<FixedSizeBlock *>(aligned_alloc(8, sizeof(FixedSizeBlock)));
    ;  // static_cast<FixedSizeBlock
       // *>(std::aligned_alloc(8,sizeof(FixedSizeBlock)));//new
       // FixedSizeBlock();
    new_block->initVal();
    new_block->initBlockArr();
    IndrArray[++curIndex] = new_block;

    /*

for(int i=0;i<(blockLen * fixedSize);i++)
{
blockArr[i] = NULL;
}

*/
  } else {
    int blenprev = curSize;
    int b_len_new = 2 * curSize;

    FixedSizeBlock **temp = static_cast<FixedSizeBlock **>(
        aligned_alloc(8, 8 * ((b_len_new)*fixedSize_queue)));
    ;  // static_cast<FixedSizeBlock
       // **>(std::aligned_alloc(8,8 *
       // ((b_len_new) * fixedSize_queue)));
    memset(temp, 0, ((b_len_new)*fixedSize_queue));

    //  FixedTimeIndexBlock **temp2;
    // todo deletion mechanism ()create node in globle linklist and add the
    // address of deleted index array and with time to replace the garbage
    // collector will deallocate the memory than int k=0; int tempC = (blenprev)
    // * fixedSize;
    int bprev = (blenprev)*fixedSize_queue;
    for (int i = 0; i < bprev; i++) {
      temp[i] = IndrArray[i];
    }

    curIndex = blenprev - 1;

    void *ptr1 = IndrArray;

    IndrArray = temp;

    free(ptr1);
    // ptr->free(ptr1, bprev * 8);
    // std::free(ptr1);
    blockSize = b_len_new * fixedSize_queue;

    curSize = b_len_new;
  }
}