#ifndef VERSIONBLOCK_H
#define VERSIONBLOCK_H

#include <data-structure/data_types.h>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/concurrent_unordered_set.h>
#include <tbb/concurrent_vector.h>
#include <tbb/spin_rw_mutex.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <vector>

#include "Transaction/Transaction.h"
// #include "jemalloc/jemalloc.h"
#include "utils/Interface.h"

#define READ_WRITE_TXN (1L << 63)
#define HAS_READ_WRITE_MARK(x) (x & READ_WRITE_TXN)
#define SET_READ_WRITE_MARK(x) (x |= READ_WRITE_TXN)
#define UN_READ_WRITE_MARK(x) (x & ~READ_WRITE_TXN)

class VersionBlock;

class VersionBlockManager;

class VersionEntry {
 public:
  unsigned offset, merge_times;
  epoch_t last_epoch;
  dst_t txn;
  dst_t edge, weight;
  VersionLink link;
  VersionEntry(dst_t e) : edge(e) {}

  VersionEntry() : edge(0) {}

  VersionEntry(dst_t e, uint64_t weight, Transaction* txn_)
      : txn(txn_->get_version()), edge(e), weight(weight) {
    if (!txn_->is_write_only()) SET_READ_WRITE_MARK(txn);
  }

  bool HasReadWriteTxn() { return txn & READ_WRITE_TXN; }
};

struct EdgeItem {
  dst_t e;
  dst_t properties;
  VersionLink link;
  // uint64_t weight;
  // VersionLink link2;
};

struct EdgeWithIndex {
  union {
    dst_t e;
    dst_t properties;
  };
  union {
    struct {
      epoch_t block;
      unsigned index;
    };
    VersionBlock* link;
  };

  void Set(dst_t e, epoch_t block, unsigned index) {
    this->e = e;
    this->block = block;
    this->index = index;
  }

  void Set(dst_t p, VersionBlock* link) {
    this->properties = p;
    this->link = link;
  }

  bool HasTmpVB() { return block & TMPVB_MASK; }
  epoch_t GetLinkBlock() { return block & ~TMPVB_MASK; }

  void clear() { link = 0; }
};

/**
 * @brief VersionBlock is a block of versions of a vertex
 * NOTE: main class
 *
 * four operation :
 *  1. InsertVersion : Transaction insert a version data in a temporary-VB
 *  2. Transform : transform a temporary-VB to a constructed-VB
 *  3. Sink : adjust a constructed-VB caused by new transforming process
 *  4. Read : read a version data from a constructed-VB, using VBIterator
 *
 * Manager by VersionBlockManager
 */

class TmpVersionBlock {
  friend class VersionBlock;

 public:
  unsigned tmp_num, version_num_;

  VBManagerInterface* manager_;

  MyEdgeBlockInterface* edge_block_;
  epoch_t timestamp_;
  VersionEntry* tmp_entry_;
  void Clear() {
    version_num_ = 0;
    tmp_num = 0;
    tmp_entry_ = nullptr;
    // delete[] tmp_entry_;
  }

  // void Clear() { version_num_ = 0; }
  TmpVersionBlock()
      : timestamp_(0), version_num_(0), tmp_num(0), tmp_entry_(nullptr) {
          // tmp_entry_ = reinterpret_cast<VersionEntry
          // *>(sizeof(VersionEntry) * 5);
          // memset(tmp_entry_, 0, sizeof(VersionEntry) * 5);
          // tmp_entry_ = nullptr;  // reinterpret_cast<VersionEntry
          // *>(la->Allocate(8,
          // 1));
          //     reinterpret_cast<VersionEntry *>(malloc(8 *
          //     sizeof(VersionEntry)));
          // tmp_num = 8;
          // tmp_entry_.reserve(5);
        };

  void SetEdgeBlock(MyEdgeBlockInterface* edge_block) {
    edge_block_ = edge_block;
  }
  MyEdgeBlockInterface* GetEdgeBlock() { return edge_block_; }
  epoch_t GetTimestamp() { return timestamp_; }
  void SetTimestamp(epoch_t ts) { timestamp_ = ts; }
  unsigned InsertVersion(dst_t* edge, epoch_t last_epoch, unsigned last_index,
                         VersionBlock* vb, Transaction* txn, unsigned offset,
                         unsigned mt, epoch_t le);
  bool ChangeVersion(dst_t* edge, unsigned index, Transaction* txn, unsigned mt,
                     unsigned offset);
  void ChangeNextIndex(unsigned index, epoch_t next_epoch, unsigned next_index);
  void ChangeLastIndex(unsigned index, VersionBlock* vb);
  void ChangeOffset(unsigned index, unsigned offset);
  unsigned GetVersionNum() { return version_num_; }
};

class VersionBlock {
 public:
  friend class VBIterator;

  // used for manage transaction
  int edge_num_ = 0;
  unsigned version_num_;
  unsigned readts_num_;
  unsigned end_;  // used for sink
  // Record basic information of the VersionBlock
  epoch_t timestamp_;
  epoch_t last_epoch_;
  VBManagerInterface* manager_;

 public:
  VersionBlock* last_vb_;  // used for fast find next VersionBlock in VBIterator
  /**
   * start of version block
   * fixed after Transform
   */
  EdgeWithIndex* start_;

  dst_t src_;

  VersionBlock(TmpVersionBlock* tvb);

  void SetNextVB(VersionBlock* next, epoch_t ts);

  // bool GetReadMutex();
  // void ReleaseReadMutex();
};

#endif  // VERSIONBLOCK_H