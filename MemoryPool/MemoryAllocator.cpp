#include "MemoryAllocator.h"

/* PerThreadMemoryAllocator class defination */
void PerThreadMemoryAllocator::init(unsigned fixedLens_, int space,
                                    int total_thread) {
  /*

   8
   16
   32 more frequent block request
   40 more frequent block request
   64
   128
   256
   512
   1024
   2048
   4096
   8192


   // one thing I understand for bigger datasets graph500-26 I should miss the
   hashtable for element <= 32
   */

  // int BlockSize[15] = {8,   16,  24,  32,   40,   48,   56,  64,
  //                      128, 256, 512, 1024, 2048, 4096, 8192};
  // int spaceNeeded[15] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};

  // block = static_cast<HashTableMemoryIndex *>(
  //     aligned_alloc(8, sizeof(HashTableMemoryIndex)));
  // new (block) HashTableMemoryIndex(NULL);

  // for (int i = 0; i < 15; i++) {
  fixedLens = fixedLens_;
  //   AllocatorBlock *ptr = new AllocatorBlock();

  TwoLevelVector *vec_two =
      static_cast<TwoLevelVector *>(aligned_alloc(8, sizeof(TwoLevelVector)));
  vec_two->initVal(fixedLens);
  vec_two->setIndrArray();

  //   GlobalAllocator *gl =
  //       static_cast<GlobalAllocator *>(aligned_alloc(8,
  //       sizeof(GlobalAllocator)));

  //   gl->init((spaceNeeded[i]), true);  // space should be block dependent

  //   vec_two->insert(gl);

  block.memBlock = vec_two;
  block.freeList = nullptr;
  block.isAvailable = false;

  //   block->insert(BlockSize[i], ptr, NULL);
  //   block->getIndexVal(BlockSize[i])->isAvailable = false;
  // }

  //   for (int i = 0; i < 11; i++) {
  //     AllocatorBlock *ptr = new AllocatorBlock();

  //     TwoLevelVector *vec_two =
  //         static_cast<TwoLevelVector *>(aligned_alloc(8,
  //         sizeof(TwoLevelVector)));
  //     vec_two->initVal();
  //     vec_two->setIndrArray();

  //     GlobalAllocator *gl = static_cast<GlobalAllocator *>(
  //         aligned_alloc(8, sizeof(GlobalAllocator)));

  //     if (((std::size_t)1 << (i + 14)) <= SIXTEENMB &&
  //         ((std::size_t)1 << (i + 14)) > EIGHTMB) {
  //       gl->init(16, true);  // space should be block dependent
  //     } else if (((std::size_t)1 << (i + 14)) > FOURMB &&
  //                ((std::size_t)1 << (i + 14)) <= EIGHTMB) {
  //       gl->init(8, true);  // space should be block dependent

  //     } else {
  //       gl->init(4, true);  // space should be block dependent
  //     }
  //     vec_two->insert(gl);

  //     ptr->memBlock = vec_two;

  //     block->insert(((std::size_t)1 << (i + 14)), ptr, NULL);
  //     block->getIndexVal((std::size_t)1 << (i + 14))->isAvailable = false;
  //   }

  //   // Hash table resize block that are not power of two
  //   for (int i = 0; i < 9; i++) {
  //     MemoryBucket *m1 = block->getIndexVal(PRIMETABLE[i] * 8);
  //     if (m1 == NULL) {
  //       AllocatorBlock *ptr = new AllocatorBlock();

  //       TwoLevelVector *vec_two = static_cast<TwoLevelVector *>(
  //           aligned_alloc(8, sizeof(TwoLevelVector)));
  //       vec_two->initVal();
  //       vec_two->setIndrArray();

  //       GlobalAllocator *gl = static_cast<GlobalAllocator *>(
  //           aligned_alloc(8, sizeof(GlobalAllocator)));
  //       if ((PRIMETABLE[i] * 8) <= SIXTEENMB && (PRIMETABLE[i] * 8) >
  //       EIGHTMB) {
  //         gl->init(16, true);  // space should be block dependent
  //       } else if ((PRIMETABLE[i] * 8) > FOURMB &&
  //                  ((std::size_t)1 << (i + 14)) <= EIGHTMB) {
  //         gl->init(8, true);  // space should be block dependent

  //       } else {
  //         gl->init(4, true);  // space should be block dependent
  //       }

  //       vec_two->insert(gl);

  //       ptr->memBlock = vec_two;

  //       block->insert((PRIMETABLE[i] * 8), ptr, NULL);

  //       block->getIndexVal((PRIMETABLE[i] * 8))->isAvailable = false;
  //     }
  //   }
}

void *PerThreadMemoryAllocator::Allocate() {
  //   if (requestedBlock <= SIXTEENMB) {

  /*  if(m == NULL)
    {
        std::cout<<"the m is null"<<" "<<requestedBlock<<std::endl;
    }
      if(block.getBlockSize() != requestedBlock)
      {
          std::cout<<"the block size does not match"<<std::endl;
      }*/
  if (!block.isAvailable) {
    return block.memBlock->getBlock();
  } else {
    auto q = block.freeList;

    if (q->empty()) {
      return block.memBlock->getBlock();
    } else {
      return q->get_block();
    }
  }
}

void PerThreadMemoryAllocator::free(void *ptr) {
  TwoLevelQueue *two = block.freeList;
  if (two == nullptr) {
    ///
    TwoLevelQueue *newB =
        static_cast<TwoLevelQueue *>(aligned_alloc(8, (sizeof(TwoLevelQueue))));
    newB->initVal();
    newB->setIndrArray();
    newB->insert(ptr);
    block.freeList = newB;
    block.isAvailable = true;
    ////
  } else {
    two->insert(ptr);
  }

  // block->getIndexVal(deletedBlock)->getFreeBlockList()->lock.unlock();
}

/* MemoryAllocator class defination */
void MemoryAllocator::init(unsigned fixedLens_, int totalSpace,
                           int totalThread) {
  fixedLens = fixedLens_;
  int per_thread_space = totalSpace / totalThread;
  // std::cout<<
  // todo perthreadalloc
  perThreadArr =
      static_cast<PerThreadMemoryAllocator **>(malloc(totalThread * 8));

  for (int i = 0; i < totalThread; i++) {
    PerThreadMemoryAllocator *ptr = static_cast<PerThreadMemoryAllocator *>(
        aligned_alloc(8, sizeof(PerThreadMemoryAllocator)));
    ptr->init(fixedLens_, per_thread_space, totalThread);
    ptr->thread_id = i;
    perThreadArr[i] = ptr;
  }
}

PerThreadMemoryAllocator *MemoryAllocator::getPerThreadMemoryAllocator(
    int thread_id) {
  return perThreadArr[thread_id];
}

void *MemoryAllocator::Allocate(int thread_id) {
  // std::cout<<thread_id<<std::endl;

  // perThreadArr[thread_id]->lock.lock();
  void *ptr;

  ptr = perThreadArr[thread_id]->Allocate();

  // perThreadArr[thread_id]->lock.unlock();

  //  perThreadArr[thread_id]->lock.unlock();

  return ptr;
}

void MemoryAllocator::free(void *ptr, int thread_id) {
  /*  if(deletedBlock == 32)
    {
        counter++;
    }
    */

  perThreadArr[thread_id]->free(ptr);

  // perThreadArr[thread_id]->lock.unlock();

  // perThreadArr[thread_id]->free(ptr, deletedBlock, isHash);
}