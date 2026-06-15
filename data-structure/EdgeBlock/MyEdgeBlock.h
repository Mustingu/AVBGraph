#ifndef EDGE_BLOCK_H
#define EDGE_BLOCK_H

#include <utils/utils.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <unordered_map>

#include "HashTable.h"
#include "RWSpinLock.h"
#include "VersionBlockManager.h"
#include "gapbs.h"
#define TMPNUM 64

/**
 * Represents a block of memory which contains edges, versions and properties.
 *
 * The block starts with edges, interleaved with versions and ends with
 * properties. The edges and versions grow upwards and the properties grow
 * downwards.
 *
 * In the unversioned case, the property value belonging to an edge has the same
 * offset in the property section. In the versioned case, the property value
 * belonging to an edge has the same offset in the property section minus all
 * versions that are before the edge in question. This allows random access to
 * edge and property in the unversioned case but requires scanning edges from
 * the beginning in the versioned case.
 *
 * We do not support multiple property versions yet. They can be supported the
 * same way as supporting edge versions but requires to clean the property
 * section on GC.
 */
class MyEdgeBlock : public MyEdgeBlockInterface {
 public:
  MyEdgeBlock(dst_t src, dst_t* _start, size_t _capacity,
              size_t _edges_and_versions);

  MyEdgeBlock();
  MyEdgeBlock(dst_t src);

  void setSrc(dst_t src);
  MyEdgeBlock(const MyEdgeBlock& other);

  size_t get_edges_and_versions() { return edges_and_versions; }
  VersionBlockManager& GetVBM() { return VBM; }

  // Accessors for testing / low-level access
  EdgeWithIndex* get_start() { return start; }
  unsigned get_capacity() { return capacity; }
  EdgeWithIndex* get_tmp_item() { return tmp_item; }
  unsigned get_tmp_ev() { return tmp_ev; }
  TmpVersionBlock** get_tmp_vb() { return tmp_vb; }

  // char* properties_end() { return ; }
  /**
   * Finds the correct place to add edge, version record and properties and
   * inserts them by shifthing.
   *
   * @param e
   * @param version
   * @param properties
   */

  unsigned Transform(TmpVersionBlock* tvb, VersionBlock* vb);

  void merge_tmpev_with_eb();
  bool get_edge_index2(dst_t e, EdgeWithIndex*& item_e, EdgeWithIndex*& item_p);
  bool get_edge_index(dst_t e, EdgeWithIndex*& item_e, EdgeWithIndex*& item_p,
                      unsigned& offset);
  // TODO: adjust to Versionblock
  bool insert_edge_block(dst_t* e, epoch_t epoch, Transaction* txn,
                         bool new_entry);

  unsigned get_degree() { return num; }
  unsigned get_degree(epoch_t epoch);
  // unsigned get_degree();
  void build(unsigned _num, dst_t* _edges, dst_t* _properties);

  void my_print_block();

  // NOTE: build this function for test
  double getSum(dst_t src,
                std::map<std::pair<dst_t, dst_t>, std::vector<unsigned>>&
                    edge_wight_versioned,
                epoch_t epoch, bool& need_iterator, VersionBlock*& vb,
                std::vector<dst_t>& pmHM);
  /*
  bool delete_edge(dst_t e, version_t version);*/

  /**
   * Finds the upper bound for value in a sorted array.
   *
   * Ignores versions.
   *
   * @param start
   * @param end
   * @param value
   * @return a pointer to the position of the upper bound or end.
   * @return a pointer to the position of the upper bound or end.
   */
  dst_t* find_upper_bound(dst_t* start, dst_t* end, dst_t value);

  void getReadLock();
  void unleashReadLock();

  dst_t get_src();

  template <typename EdgeCallback>
  void iterate_edges(epoch_t epoch, bool& need_iterator, VersionBlock*& vb,
                     EdgeCallback callback) {
    // volatile double sm = 0;
    for (auto i = tmp_item; i < tmp_item + tmp_ev; i++) {
      if (!is_delete(i->e) && i->GetLinkBlock() <= epoch) {
        callback(i);
      }
    }

    for (auto i = start; i < start + edges_and_versions; i++) {
      if (!is_delete(i->e) && i->GetLinkBlock() <= epoch) {
        callback(i);
      }
    }
    // uint64_t* k = reinterpret_cast<uint64_t*>(tmp_item);
    // for (auto i = 0; i < tmp_ev; i++) {
    //   // if (!is_delete(tmp_item[i].e) && tmp_item[i].GetLinkBlock() <=
    //   // epoch) {
    //   callback(k + i);
    //   // }
    // }
    // k = reinterpret_cast<uint64_t*>(start);
    // for (auto i = 0; i < edges_and_versions; i++) {
    //   // if (!is_delete(i->e) && i->GetLinkBlock() <= epoch) {
    //   // callback(i);
    //   callback(k + i);
    //   // sm += 1;
    //   // }
    // }

#ifdef FINEGRAIN
    if (epoch <= VBM.GetEndEpoch()) {
#else
    if (epoch < VBM.GetEndEpoch()) {
#endif
      need_iterator = true;
      vb = VBM.GetLastVB();
    }
  }

  template <typename EdgeCallback>
  void iterate_edges_condition(epoch_t epoch, bool& need_iterator,
                               VersionBlock*& vb, EdgeCallback callback) {
    // volatile double sm = 0;
    for (auto i = tmp_item; i < tmp_item + tmp_ev; i++) {
      if (!is_delete(i->e) && i->GetLinkBlock() <= epoch) {
        if (callback(i)) return;
      }
    }

    for (auto i = start; i < start + edges_and_versions; i++) {
      if (!is_delete(i->e) && i->GetLinkBlock() <= epoch) {
        if (callback(i)) return;
      }
    }
    // uint64_t* k = reinterpret_cast<uint64_t*>(tmp_item);
    // for (auto i = 0; i < tmp_ev; i++) {
    //   // if (!is_delete(tmp_item[i].e) && tmp_item[i].GetLinkBlock() <=
    //   // epoch) {
    //   callback(k + i);
    //   // }
    // }
    // k = reinterpret_cast<uint64_t*>(start);
    // for (auto i = 0; i < edges_and_versions; i++) {
    //   // if (!is_delete(i->e) && i->GetLinkBlock() <= epoch) {
    //   // callback(i);
    //   callback(k + i);
    //   // sm += 1;
    //   // }
    // }

    if (epoch < VBM.GetEndEpoch()) {
      need_iterator = true;
      vb = VBM.GetLastVB();
    }
  }

  template <typename EdgeCallback>
  void iterate_edges_with_property(epoch_t epoch, bool& need_iterator,
                                   VersionBlock*& vb, EdgeCallback callback) {
    // volatile double sm = 0;
    for (auto i = tmp_item; i < tmp_item + tmp_ev; i++) {
      if (!is_delete(i->e) && i->GetLinkBlock() <= epoch) {
        // if (src_ == 43 || src_ == 36) {
        // std::cout << "src: " << src_ << " e: " << i->e
        //           << " p: " << *(double*)(&(i + TMPNUM)->properties) << " "
        //           << (i->block & ~TMPVB_MASK) << " "
        //           << ((i + TMPNUM)->block & ~TMPVB_MASK) << '\n';
        // }
        callback(i, i + TMPNUM);
      }
    }

    for (auto i = start; i < start + edges_and_versions; i++) {
      if (!is_delete(i->e) && i->GetLinkBlock() <= epoch) {
        // if (src_ == 43 || src_ == 36) {
        // std::cout << "src: " << src_ << " e: " << i->e
        //           << " p: " << *(double*)(&(i + TMPNUM)->properties) << " "
        //           << (i->block & ~TMPVB_MASK) << " "
        //           << ((i + TMPNUM)->block & ~TMPVB_MASK) << '\n';
        // }
        callback(i, i + capacity);
      }
    }

    if (epoch < VBM.GetEndEpoch()) {
      need_iterator = true;
      vb = VBM.GetLastVB();
    }
  }

  unsigned GetSrc() { return src_; }

 private:
  int edge_num_ = 0;
  unsigned tmp_ev = 0;
  unsigned merge_times = 0;
  unsigned edges_and_versions, num;
  unsigned capacity = 0;

  HashTable2 M;
  dst_t src_;

  tbb::spin_rw_mutex spin_rw_lock;

  VersionBlockManager VBM;

  // EdgeItem* start;
  // EdgeItem* tmp_item;
  EdgeWithIndex *start, *tmp_item;

  TmpVersionBlock* tmp_vb[MAXSIMULBATCH];
};

class GraphAlgorithms {
 public:
  template <typename EdgeCallback>
  static void for_each_edge(MyEdgeBlock* eb, epoch_t epoch,
                            EdgeCallback callback,
                            Satistical* count = nullptr) {
    if (!eb) return;
    bool need_iterator = false;
    VersionBlock* vb = nullptr;
    eb->iterate_edges(epoch, need_iterator, vb, callback);

    if (need_iterator) {
      // std::cout << "\nyes : " << vb.get() << '\n';
      auto it = new VBIterator(vb, epoch, count);
      while (!it->is_end_) {
        // std::cout << "in while : " << it->vb_.get() << '\n';
        auto start = it->vb_->start_;
        auto n = it->vb_->version_num_;
        int i = 0;
#ifdef FINEGRAIN
        if (it->vb_->timestamp_ == epoch) {
          for (i = 0; i < n && (start + i)->block <= epoch; i++) {
            if (count != nullptr) {
              count->reach_record();
            }
            callback(start + i);
          }
        } else {
#endif
          for (i = 0; i < n && (start + i)->block <= epoch; i++) {
            // auto os = MEA->p_mHashMap[i],
            //      od = MEA->p_mHashMap[start->e & ~DELETION_MASK];
            // auto it =
            //     os < od ? std::make_pair<>(os, od) : std::make_pair<>(od,
            //     os);
            // if (edge_wight_versioned.find(it) == edge_wight_versioned.end())
            //   std::cout << "there is not in edge_wight_versioned in VB: "
            //             << it.first << " " << it.second << " "
            //             << (*reinterpret_cast<double*>(&start->properties))
            //             << '\n';
            // callback(&start->e);
            callback(start + i);
            // if (count != nullptr) {
            //   count->reach_record();
            //   count->reach_visible();
            // }
          }
#ifdef FINEGRAIN
        }
#endif

        // if (count != nullptr && i != n) {
        //   // std::cout << "Yes\n";
        //   count->reach_record();
        // }
        //  else {
        //   std::cout << std::to_string(n) + " " +
        //                    std::to_string((start + n - 1)->block) + " " +
        //                    std::to_string(epoch) + " " +
        //                    std::to_string(it->vb_->timestamp_) + '\n';
        // }

        it->getNext();
      }
      delete it;
    }
  }

  template <typename EdgeCallback>
  static void for_each_edge(MyEdgeBlock* eb, epoch_t epoch,
                            EdgeCallback callback) {
    if (!eb) return;
    bool need_iterator = false;
    VersionBlock* vb = nullptr;
    eb->iterate_edges(epoch, need_iterator, vb, callback);

    if (need_iterator) {
      // std::cout << "\nyes : " << vb.get() << '\n';
      auto it = new VBIterator(vb, epoch);
      while (!it->is_end_) {
        // std::cout << "in while : " << it->vb_.get() << '\n';
        auto start = it->vb_->start_;
        auto n = it->vb_->version_num_;
        for (int i = 0; i < n && (start + i)->block <= epoch; i++) {
          // auto os = MEA->p_mHashMap[i],
          //      od = MEA->p_mHashMap[start->e & ~DELETION_MASK];
          // auto it =
          //     os < od ? std::make_pair<>(os, od) : std::make_pair<>(od,
          //     os);
          // if (edge_wight_versioned.find(it) == edge_wight_versioned.end())
          //   std::cout << "there is not in edge_wight_versioned in VB: "
          //             << it.first << " " << it.second << " "
          //             << (*reinterpret_cast<double*>(&start->properties))
          //             << '\n';
          // callback(&start->e);
          callback(start + i);
        }

        it->getNext();
      }
      delete it;
    }
  }

  template <typename EdgeCallback>
  static void for_each_edge_condition(MyEdgeBlock* eb, epoch_t epoch,
                                      EdgeCallback callback) {
    if (!eb) return;
    bool need_iterator = false;
    VersionBlock* vb = nullptr;
    eb->iterate_edges_condition(epoch, need_iterator, vb, callback);

    if (need_iterator) {
      // std::cout << "yes : " << vb->timestamp_ << " " << epoch << '\n';
      auto it = new VBIterator(vb, epoch);
      while (!it->is_end_) {
        // std::cout << "in while : " << it->vb_.get() << '\n';
        auto start = it->vb_->start_;
        auto n = it->vb_->version_num_;
        for (int i = 0; i < n && (start + i)->block <= epoch; i++) {
          // auto os = MEA->p_mHashMap[i],
          //      od = MEA->p_mHashMap[start->e & ~DELETION_MASK];
          // auto it =
          //     os < od ? std::make_pair<>(os, od) : std::make_pair<>(od,
          //     os);
          // if (edge_wight_versioned.find(it) == edge_wight_versioned.end())
          //   std::cout << "there is not in edge_wight_versioned in VB: "
          //             << it.first << " " << it.second << " "
          //             << (*reinterpret_cast<double*>(&start->properties))
          //             << '\n';
          // callback(&start->e);
          if (callback(start + i)) {
            delete it;
            return;
          }
        }

        it->getNext();
      }
      delete it;
    }
  }
  template <typename EdgeCallback>
  static void for_each_edge_with_property(MyEdgeBlock* eb, epoch_t epoch,
                                          EdgeCallback callback) {
    if (!eb) return;
    bool need_iterator = false;
    VersionBlock* vb = nullptr;
    eb->iterate_edges_with_property(epoch, need_iterator, vb, callback);

    if (need_iterator) {
      // std::cout << "\nyes : " << vb.get() << '\n';
      auto it = new VBIterator(vb, epoch);
      while (!it->is_end_) {
        // std::cout << "in while : " << it->vb_.get() << '\n';
        auto start = it->vb_->start_;
        auto n = it->vb_->version_num_;
        for (int i = 0; i < n && (start + i)->block <= epoch; i++) {
          // auto os = MEA->p_mHashMap[i],
          //      od = MEA->p_mHashMap[start->e & ~DELETION_MASK];
          // auto it =
          //     os < od ? std::make_pair<>(os, od) : std::make_pair<>(od,
          //     os);
          // if (edge_wight_versioned.find(it) == edge_wight_versioned.end())
          //   std::cout << "there is not in edge_wight_versioned in VB: "
          //             << it.first << " " << it.second << " "
          //             << (*reinterpret_cast<double*>(&start->properties))
          //             << '\n';
          // callback(&start->e);
          callback(start + i, start + i + n);
          // std::cout << "In Ieterator: " << eb->GetSrc() << " " << (start +
          // i)->e
          //           << " " << (start + i)->block << '\n';
        }

        it->getNext();
      }
      delete it;
    }
  }
};

#endif  // EDGE_BLOCK_H