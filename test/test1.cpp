//
// Created by masitan on 11/22/24.
//

#include "test1.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>

// #include <boost/asio.hpp>
// #include <boost/container/flat_map.hpp>
#include <deque>
#include <map>

#include "../third-party/RWSpinLock.h"
#include "BloomFilter.h"
#include "gettimetest.h"
// #include "jemalloc/jemalloc.h"
#include "MemoryPool/MemoryAllocator.h"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/spin_mutex.h"
#include "tbb/spin_rw_mutex.h"
#include "tbb/task_arena.h"
// #include "tbb_concurrent_map_test.h"
const uint64_t FLAG_EMPTY_SLOT = 0xFFFFFFFFFFFFFFFF;
const uint64_t FLAG_TOMB_STONE = 0xFFFFFFFFFFFFFFFE;

class HashTable {
  uint64_t* hashtable;
  unsigned entryCounter;
  unsigned hashBlockSize;

  uint64_t hash(uint64_t key) { return key % hashBlockSize; }

 public:
  HashTable() {
    hashtable = static_cast<uint64_t*>(malloc(16));
    hashBlockSize = 2;
    memset(hashtable, FLAG_EMPTY_SLOT, sizeof(uint64_t) * hashBlockSize);

    entryCounter = 0;
  }

  bool getDestHashTableVal(uint64_t dest_node) {
    // get the dest node
    size_t index = hash(dest_node);
    //  int count = 0;
    while (hashtable[index] != FLAG_EMPTY_SLOT) {
      //   count++;
      if (hashtable[index] == dest_node) {
        return true;
      }
      index = index + 1;
      if (index >= hashBlockSize) {
        index = 0;
      }
    }
    return false;
  }

  void resizeHashTable(int requested_block) {
    // HashTablePerSource* oldArr = hashPtr;

    u_int32_t oldCap = hashBlockSize;

    // capacity =requested_block;
    // u_int64_t* newHashTable = static_cast<u_int64_t
    // *>(la->Allocate(sizeof(HashTablePerSource), thread_id));

    u_int64_t* newHashTable =
        static_cast<u_int64_t*>(malloc(sizeof(u_int64_t) * requested_block));

    memset(newHashTable, FLAG_EMPTY_SLOT,
           requested_block * sizeof(u_int64_t));  // reset new array
    hashBlockSize = requested_block;

    // newHashTable->hashBlockSize = requested_block;

    for (u_int32_t i = 0; i < oldCap; i++) {
      u_int64_t adj_p = hashtable[i];
      if (adj_p != FLAG_EMPTY_SLOT) {
        u_int64_t index = hash(adj_p);
        while (newHashTable[index] != FLAG_EMPTY_SLOT) {
          index++;
          if (index >= requested_block) {
            index = 0;
          }
        }
        newHashTable[index] = adj_p;
      }
    }

    u_int64_t* oldArr = hashtable;
    hashtable = newHashTable;

    free(oldArr);
    // for resize everytime you need to move to the STAL to get dest node and
    // store it in the array
  }

  void setDestHashTableVal(uint64_t dest_node) {
    if (entryCounter >= ((uint32_t)hashBlockSize >> 1)) {
      resizeHashTable(hashBlockSize * 2);
    }

    entryCounter++;

    size_t index = hash(dest_node);
    // int count =0;
    while (hashtable[index] != FLAG_EMPTY_SLOT) {
      // count++;
      index++;
      if (index >= hashBlockSize) {
        index = 0;
      }
    }
    hashtable[index] = dest_node;
  }
  // ~HashTable() {}
};

class HashTable2 {
  uint64_t* hashtable;
  uint64_t* ptr;
  unsigned entryCounter;
  unsigned hashBlockSize;

  uint64_t hash(uint64_t key) { return key % hashBlockSize; }

 public:
  HashTable2() {
    hashtable = static_cast<uint64_t*>(malloc(16));
    ptr = static_cast<uint64_t*>(malloc(16));
    hashBlockSize = 2;
    memset(hashtable, FLAG_EMPTY_SLOT, sizeof(uint64_t) * hashBlockSize);

    entryCounter = 0;
  }

  bool getDestHashTableVal(uint64_t dest_node, uint64_t& link) {
    // get the dest node
    size_t index = hash(dest_node);
    //  int count = 0;
    while (hashtable[index] != FLAG_EMPTY_SLOT) {
      //   count++;
      if (hashtable[index] == dest_node) {
        link = ptr[index];
        return true;
      }
      index = index + 1;
      if (index >= hashBlockSize) {
        index = 0;
      }
    }
    return false;
  }

  void resizeHashTable(int requested_block) {
    // HashTablePerSource* oldArr = hashPtr;

    u_int32_t oldCap = hashBlockSize;

    // capacity =requested_block;
    // u_int64_t* newHashTable = static_cast<u_int64_t
    // *>(la->Allocate(sizeof(HashTablePerSource), thread_id));

    u_int64_t* newHashTable =
        static_cast<u_int64_t*>(malloc(sizeof(u_int64_t) * requested_block));
    u_int64_t* newPtr =
        static_cast<u_int64_t*>(malloc(sizeof(u_int64_t) * requested_block));

    memset(newHashTable, FLAG_EMPTY_SLOT,
           requested_block * sizeof(u_int64_t));  // reset new array
    hashBlockSize = requested_block;

    // newHashTable->hashBlockSize = requested_block;

    for (u_int32_t i = 0; i < oldCap; i++) {
      u_int64_t adj_p = hashtable[i];
      if (adj_p != FLAG_EMPTY_SLOT) {
        u_int64_t index = hash(adj_p);
        while (newHashTable[index] != FLAG_EMPTY_SLOT) {
          index++;
          if (index >= requested_block) {
            index = 0;
          }
        }
        newHashTable[index] = adj_p;
        newPtr[index] = ptr[i];
      }
    }

    u_int64_t* oldArr = hashtable;
    std::swap(ptr, newPtr);
    hashtable = newHashTable;

    free(oldArr);
    free(newPtr);
    // for resize everytime you need to move to the STAL to get dest node and
    // store it in the array
  }

  void setDestHashTableVal(uint64_t dest_node, uint64_t link) {
    if (entryCounter >= ((uint32_t)hashBlockSize >> 1)) {
      resizeHashTable(hashBlockSize * 2);
    }

    entryCounter++;

    size_t index = hash(dest_node);
    // int count =0;
    while (hashtable[index] != FLAG_EMPTY_SLOT) {
      // count++;
      index++;
      if (index >= hashBlockSize) {
        index = 0;
      }
    }
    hashtable[index] = dest_node;
    ptr[index] = link;
  }
  // ~HashTable() {}
};

class Node1 {
  std::map<int, bool> M;
  int tmp_ev[64], tmp_nm;

 public:
  Node1() { tmp_nm = 0; }
  void insert(int i) {
    if (tmp_nm == 64) {
      tmp_nm = 0;
    }
    auto it = M.find(i);
    if (it == M.end()) {
      M[i] = true;
      tmp_ev[tmp_nm++] = i;
    }
  }
  uint64_t getSum() {
    uint64_t sum = 0;
    for (int i = 0; i < tmp_nm; i++) {
      sum += tmp_ev[i];
    }
    return sum;
  }
} A1[128];

class Node2 {
  HashTable M;
  int tmp_ev[64], tmp_nm;

 public:
  Node2() { tmp_nm = 0; }
  void insert(int i) {
    if (tmp_nm == 64) {
      tmp_nm = 0;
    }
    auto it = M.getDestHashTableVal(i);
    if (!it) {
      M.setDestHashTableVal(i);
      tmp_ev[tmp_nm++] = i;
    }
  }
  uint64_t getSum() {
    uint64_t sum = 0;
    for (int i = 0; i < tmp_nm; i++) {
      sum += tmp_ev[i];
    }
    return sum;
  }
} A2[128];

class Node3 {
  HashTable2 M;
  int tmp_ev[64], tmp_nm;

 public:
  Node3() { tmp_nm = 0; }
  void insert(int i) {
    if (tmp_nm == 64) {
      tmp_nm = 0;
    }
    uint64_t f = 0;
    auto it = M.getDestHashTableVal(i, f);
    if (!it) {
      M.setDestHashTableVal(i, i);
      tmp_ev[tmp_nm++] = i;
    }
  }
  uint64_t getSum() {
    uint64_t sum = 0;
    for (int i = 0; i < tmp_nm; i++) {
      sum += tmp_ev[i];
    }
    return sum;
  }
} A3[128];

const int num = 2560000;
int src[num + 1], dst[num + 1];

void Data_initailize() {
  for (int i = 0; i < num; i++) {
    src[i] = rand() % 128;
    dst[i] = rand() % 128;
  }
}

void F1() {
  for (int i = 0; i < num; i++) {
    A1[src[i]].insert(dst[i]);
  }
  int sm = 0;
  for (int i = 0; i < 128; i++) {
    sm += A1[i].getSum();
  }
  std::cout << sm << '\n';
}

void F2() {
  for (int i = 0; i < num; i++) {
    A2[src[i]].insert(dst[i]);
  }
  int sm = 0;
  for (int i = 0; i < 128; i++) {
    sm += A2[i].getSum();
  }
  std::cout << sm << '\n';
}

void F3() {
  for (int i = 0; i < num; i++) {
    A3[src[i]].insert(dst[i]);
  }
  int sm = 0;
  for (int i = 0; i < 128; i++) {
    sm += A3[i].getSum();
  }
  std::cout << sm << '\n';
}
MemoryAllocator* laa;
void N1() {
  int sm = 0;
  for (int i = 0; i < num; i++) {
    unsigned* x = new unsigned(i);
    sm += *x;
    delete x;
  }
  std::cout << sm << '\n';
}
void N2() {
  int sm = 0;
  for (int i = 0; i < num; i++) {
    unsigned* x = reinterpret_cast<unsigned*>(laa->Allocate(0));
    laa->free(x, 0);
  }
  // std::cout << sm << '\n';
}
int main() {
  // std::vector<RWSpinLock> test;
  // test.emplace_back();
  // Data_initailize();
  laa = new MemoryAllocator();
  laa->init(4, 100, 1);
  PrintFunctionTime(N1, "Entire Entry");
  PrintFunctionTime(N2, "Entire Entry2");
  // PrintFunctionTime(F2, "Entire Entry2");
  // PrintFunctionTime(F3, "Entire Entry3");
}
