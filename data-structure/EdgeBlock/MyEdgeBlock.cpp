#include "MyEdgeBlock.h"

MyEdgeBlock::MyEdgeBlock(dst_t src, dst_t* _start, size_t _capacity,
                         size_t _edges_and_versions)
    : src_(src),
      start((EdgeWithIndex*)_start),
      edges_and_versions(_edges_and_versions),
      VBM(src) {
  for (int i = 0; i < MAXSIMULBATCH; i++) {
    tmp_vb[i] = VBM.tvb_array + i;
  }
};

MyEdgeBlock::MyEdgeBlock() : start(nullptr), edges_and_versions(0), VBM() {
  for (int i = 0; i < MAXSIMULBATCH; i++) {
    tmp_vb[i] = VBM.tvb_array + i;
  }
}
MyEdgeBlock::MyEdgeBlock(dst_t src)
    : src_(src), start(nullptr), edges_and_versions(0), VBM(src) {
  for (int i = 0; i < MAXSIMULBATCH; i++) {
    tmp_vb[i] = VBM.tvb_array + i;
  }
};

void MyEdgeBlock::setSrc(dst_t src) {
  src_ = src;
  VBM.setSrc(src);
}
MyEdgeBlock::MyEdgeBlock(const MyEdgeBlock& other)
    : start(other.start),
      edges_and_versions(other.edges_and_versions),
      VBM(other.src_) {
  for (int i = 0; i < MAXSIMULBATCH; i++) {
    tmp_vb[i] = VBM.tvb_array + i;
  }
}

unsigned MyEdgeBlock::Transform(TmpVersionBlock* tvb, VersionBlock* vb) {
  auto n = tvb->GetVersionNum();
  if (!n) return 0;
  std::vector<size_t> indices(n);
  std::iota(indices.begin(), indices.end(), 0);

  std::sort(indices.begin(), indices.end(),
            // 捕获 vecA1 的引用 (const&)
            [&tvb](size_t i, size_t j) {
              // 比较索引 i 和 j 处的 'a' 属性
              return tvb->tmp_entry_[i].last_epoch <
                     tvb->tmp_entry_[j].last_epoch;
            });
  auto epoch = vb->timestamp_;
  spin_rw_lock.lock();

  int i = 0;
  // if (!tvb->tmp_entry_[indices[0]].last_epoch)
  for (; i < n && !tvb->tmp_entry_[indices[i]].last_epoch; i++) {
    auto& tvb_item = tvb->tmp_entry_[indices[i]];

    EdgeWithIndex *item_e, *item_p;

    if (tvb_item.merge_times != merge_times) {
      get_edge_index2(tvb_item.edge, item_e, item_p);
    } else {
      auto xid = tvb_item.offset;
      if (xid & TMPVB_MASK) {
        item_e = tmp_item + (xid & ~TMPVB_MASK);
        item_p = item_e + TMPNUM;
      } else {
        item_e = start + xid;
        item_p = item_e + capacity;
      }
    }

    if (!tvb_item.link.HasNextVB()) {
      item_e->block = epoch;
      item_e->index = -1;
      item_p->link = nullptr;
    } else {
      item_p->link = reinterpret_cast<VersionBlock*>(
          (static_cast<uint64_t>(epoch) << 32) | (unsigned)-1);
      auto next_vb = tmp_vb[tvb_item.link.block & MODPM];
      next_vb->ChangeLastIndex(tvb_item.link.index, nullptr);
    }

    //   std::swap(item->properties, tvb_item.weight);
    item_p->properties = tvb_item.weight;

    vb->edge_num_++;
  }

  int item_num = n - i;
  if (item_num != 0)
    vb->start_ = new EdgeWithIndex[item_num << 1];
  else {
    VBM.PushVB(edge_num_, vb);
    spin_rw_lock.unlock();
    if (tvb->tmp_entry_ != nullptr) delete[] tvb->tmp_entry_;
    tvb->Clear();
    return 0;
  }

  int sm = 0;
  vb->version_num_ = item_num;

  for (int j = 0; i < n; i++, j++) {
    auto& tvb_item = tvb->tmp_entry_[indices[i]];

    if (!is_delete(tvb_item.edge))
      vb->edge_num_++;
    else
      vb->edge_num_--;

    // std::memcpy(vb->start_ + j, &tvb_item.edge, sizeof(EdgeItem));
    // vb->start_[j].Set(tvb_item.edge, tvb_item.link.block,
    // tvb_item.link.index);
    EdgeWithIndex *item_e, *item_p;

    if (tvb_item.merge_times != merge_times) {
      get_edge_index2(tvb_item.edge, item_e, item_p);
    } else {
      auto xid = tvb_item.offset;
      if (xid & TMPVB_MASK) {
        item_e = tmp_item + (xid & ~TMPVB_MASK);
        item_p = item_e + TMPNUM;
      } else {
        item_e = start + xid;
        item_p = item_e + capacity;
      }
    }
    // std::memcpy(&vb->start_[j].link, &item->link.link, sizeof(uint64_t));
    vb->start_[j].Set(tvb_item.edge, (item_e->block & ~TMPVB_MASK),
                      item_e->index);
    vb->start_[j + item_num].Set(item_p->properties, tvb_item.link.link);

    if (!tvb_item.link.HasNextVB()) {
      item_e->link = item_p->link;
      // item_p->link = vb;
      item_p->Set(tvb_item.weight, vb);
    } else {
      item_e->block = epoch;
      item_e->index = j;
      // item_p->link = reinterpret_cast<VersionBlock*>(
      //     (static_cast<uint64_t>(epoch) << 32) | j);
      auto next_vb = tmp_vb[tvb_item.link.block & MODPM];
      next_vb->ChangeLastIndex(tvb_item.link.index, vb);
    }

    // std::swap(item_p->properties, tvb_item.weight);
  }
  VBM.PushVB(edge_num_, vb);
  spin_rw_lock.unlock();
  delete[] tvb->tmp_entry_;
  tvb->Clear();
  return vb->version_num_;
}

void MyEdgeBlock::merge_tmpev_with_eb() {
  merge_times++;
  std::vector<size_t> indices(TMPNUM);
  std::iota(indices.begin(), indices.end(), 0);
  std::sort(indices.begin(), indices.end(),
            // 捕获 vecA1 的引用 (const&)
            [this](size_t i, size_t j) {
              // 比较索引 i 和 j 处的 'a' 属性
              return tmp_item[i].e < tmp_item[j].e;
            });

  bool new_edge = 0;
  int old_capacity = capacity;
  EdgeWithIndex* tmp_dst_ptr = start;
  if (num > capacity) {
    if (!capacity)
      capacity = num;
    else
      capacity <<= 1;

    new_edge = 1;
    tmp_dst_ptr = reinterpret_cast<EdgeWithIndex*>(
        std::malloc(sizeof(EdgeWithIndex) * capacity * 2));
  }
  int y = 0;
  for (int i = 0; i < edges_and_versions; i++) {
    if (!(DELETE_FLAG & start[i].e)) {
      start[y + old_capacity] = start[i + old_capacity];
      start[y++] = start[i];
    }
  }

  int x = TMPNUM - 1;
  edges_and_versions = y + TMPNUM;
  y--;
  int nw = edges_and_versions - 1;
  while (x >= 0 && y >= 0) {
    if (tmp_item[indices[x]].e > start[y].e) {
      tmp_dst_ptr[nw + capacity] = tmp_item[indices[x] + TMPNUM];
      tmp_dst_ptr[nw--] = tmp_item[indices[x--]];
    } else {
      tmp_dst_ptr[nw + capacity] = start[y + old_capacity];
      tmp_dst_ptr[nw--] = start[y--];
    }
  }

  while (x >= 0) {
    tmp_dst_ptr[nw + capacity] = tmp_item[indices[x] + TMPNUM];
    tmp_dst_ptr[nw--] = tmp_item[indices[x--]];
  }
  if (new_edge) {
    if (y >= 0) {
      memcpy(tmp_dst_ptr, start, (y + 1) * sizeof(EdgeWithIndex));
      memcpy(tmp_dst_ptr + capacity, start + old_capacity,
             (y + 1) * sizeof(EdgeWithIndex));
    }
    if (start != nullptr) free(start);
    start = tmp_dst_ptr;
  }
  tmp_ev = 0;
}
bool MyEdgeBlock::get_edge_index2(dst_t e, EdgeWithIndex*& item_e,
                                  EdgeWithIndex*& item_p) {
  int l = 0, r = edges_and_versions;
  while (l <= r) {
    int m = (l + r) >> 1;
    // t++;
    if (start[m].e > e) {
      r = m - 1;
    } else if (start[m].e < e) {
      l = m + 1;
    } else {
      item_e = start + m;
      item_p = item_e + capacity;
      return true;
    }
  }
  return false;
}
bool MyEdgeBlock::get_edge_index(dst_t e, EdgeWithIndex*& item_e,
                                 EdgeWithIndex*& item_p, unsigned& offset) {
  int l = 0, r = edges_and_versions;

  if (r != 0)
    while (l <= r) {
      int m = (l + r) >> 1;
      if (start[m].e > e) {
        r = m - 1;
      } else if (start[m].e < e) {
        l = m + 1;
      } else {
        item_e = start + m;
        item_p = item_e + capacity;
        offset = m;
        return true;
      }
    }

  for (int i = 0; i < tmp_ev; i++) {
    if (tmp_item[i].e == e) {
      item_e = tmp_item + i;
      item_p = item_e + TMPNUM;
      offset = i | TMPVB_MASK;
      return true;
    }
  }

  return false;
}
bool MyEdgeBlock::insert_edge_block(dst_t* e, epoch_t epoch, Transaction* txn,
                                    bool new_entry) {
  // return true;
  TmpVersionBlock* vb = tmp_vb[epoch & MODPM];

  spin_rw_lock.lock();

  if (tmp_ev == TMPNUM) {
    merge_tmpev_with_eb();
  }

  uint64_t link;
  EdgeWithIndex *item_e, *item_p;
  auto edge = *e;

  unsigned offset = 0;

  if (!M.getDestHashTableVal(edge, link)) {
    // edge_num_++;
    int tmp_offset = tmp_ev++;
    // memcpy(&tmp_item[tmp_offset], e, sizeof(dst_t) << 1);
    tmp_item[tmp_offset].e = edge;
    tmp_item[tmp_offset + TMPNUM].e = e[1];
    tmp_item[tmp_offset].clear();

    item_e = tmp_item + tmp_offset;
    item_p = item_e + TMPNUM;
    M.setDestHashTableVal(edge, num++);
    offset = tmp_offset | TMPVB_MASK;
  } else {
    get_edge_index(edge, item_e, item_p, offset);
  }

  if (vb->timestamp_ != epoch) {
    txn->write_lock_pushback(vb);
    vb->timestamp_ = epoch;
    vb->version_num_ = 0;
  }

  bool is_tmp = (item_e->block & TMPVB_MASK);
  epoch_t last_epoch = is_tmp ? item_p->block : item_e->block,
          last_index = is_tmp ? item_p->index : item_e->index;

  /* this RW-txn has intermedia writes during read and write epoch, which is
   illegal OR old_txn is later on timestamps layer*/
  if (!txn->is_write_only() && txn->get_read_epoch() < last_epoch ||
      last_epoch > epoch) {
    spin_rw_lock.unlock();
    return false;
  }
  if (last_epoch == epoch) {
    if (!vb->ChangeVersion(e, last_index, txn, merge_times, offset)) {
      spin_rw_lock.unlock();
      return false;
    }
  } else {
    if (is_tmp) {
      item_p->index = vb->InsertVersion(
          e, TMPVB_MASK, 0,
          reinterpret_cast<VersionBlock*>(
              (static_cast<uint64_t>(last_epoch) << 32) | last_index, epoch),
          txn, offset, merge_times, last_epoch);

      if (last_epoch != 0)
        tmp_vb[last_epoch & MODPM]->ChangeNextIndex(last_index, epoch,
                                                    item_p->index);

    } else {
      auto last_vb = item_p->link;
      // std::memcpy(&item_p->link, &item_e->block, sizeof(uint64_t));
      item_p->index = vb->InsertVersion(e, 0, 0, last_vb, txn, offset,
                                        merge_times, last_epoch);
    }

    item_e->block |= TMPVB_MASK;
    item_p->block = epoch;
  }

#ifdef FINEGRAIN
  txn->AddEB(this);
#else
  spin_rw_lock.unlock();
#endif

  return true;
};

void MyEdgeBlock::build(unsigned _num, dst_t* _edges, dst_t* _properties) {
  edges_and_versions = capacity = _num;
  num = _num;
  if (_num > 0)
    start = reinterpret_cast<EdgeWithIndex*>(
        std::malloc(sizeof(EdgeWithIndex) * _num * 2));
  else
    start = nullptr;

  auto start_p = start + _num;
  for (int i = 0; i < _num; i++) {
    start[i].e = _edges[i];
    start[i].clear();
    start_p[i].clear();
    start_p[i].properties = _properties[i];
  }

  for (int i = 0; i < MAXSIMULBATCH; i++) {
    tmp_vb[i]->SetEdgeBlock(this);
  }

  tmp_item = reinterpret_cast<EdgeWithIndex*>(
      la->Allocate(sizeof(EdgeWithIndex) * TMPNUM * 2, threadid));
  // tmp_item = reinterpret_cast<EdgeWithIndex*>(
  //     std::malloc(sizeof(EdgeWithIndex) * TMPNUM * 2));
}

void MyEdgeBlock::my_print_block() {}

unsigned MyEdgeBlock::get_degree(epoch_t epoch) {
  return (edge_num_ + VBM.getDegreeVersioned(epoch));
}

// NOTE: build this function for test
double MyEdgeBlock::getSum(
    dst_t src,
    std::map<std::pair<dst_t, dst_t>, std::vector<unsigned>>&
        edge_wight_versioned,
    epoch_t epoch, bool& need_iterator, VersionBlock*& vb,
    std::vector<dst_t>& pmHM) {
  // std::cout << "getSum src: " << pmHM[src] << '\n';
  double sm = 0;
  for (auto i = 0; i < tmp_ev; i++) {
    if (!is_delete(tmp_item[i].e) && tmp_item[i].GetLinkBlock() <= epoch) {
      // auto os = pmHM[src], od = pmHM[tmp_item[i].e & ~DELETION_MASK];
      // auto it = os < od ? std::make_pair<>(os, od) : std::make_pair<>(od,
      // os); if (edge_wight_versioned.find(it) == edge_wight_versioned.end())
      //   std::cout << "there is not in edge_wight_versioned : " << it.first
      //             << " " << it.second << " "
      //             << (*reinterpret_cast<double*>(&tmp_item[i].properties))
      //             << '\n';
      sm += (*reinterpret_cast<double*>(&tmp_item[i + TMPNUM].properties));
    }
  }

  for (auto i = start; i < start + capacity; i++) {
    if (!is_delete(i->e) && i->GetLinkBlock() <= epoch) {
      sm += (*reinterpret_cast<double*>(&(i + capacity)->properties));
    }
  }

  if (epoch < VBM.GetEndEpoch()) {
    need_iterator = true;
    vb = VBM.GetLastVB();
  }
  return sm;
}

/*
bool delete_edge(dst_t e, version_t version) {
  assert(has_space_to_delete_edge());

  auto ptr = find_upper_bound(start, start + edges_and_versions, e);
  if (ptr == start + edges_and_versions ||
      make_unversioned(*ptr) != e) {  // Edge does not exist
    return false;
  } else if (is_versioned(*ptr)) {
    int offset = ptr - start;
    int property_offset = offset - count_versions_before(offset);
    char* property = properties_start() + property_offset * property_size;
    EdgeVersionRecord vr{make_unversioned(*ptr), ptr + 1, property, true,
                         property_size};
    vr.write(version, DELETION, nullptr);
    return true;
  } else {
    memmove((char*)(ptr + 2), ptr + 1,
            (start + edges_and_versions - ptr - 1) * sizeof(dst_t));
    *ptr |= VERSION_MASK;
    *(ptr + 1) = version | DELETION_MASK;
    edges_and_versions += 1;
    return true;
  }
}*/

dst_t* MyEdgeBlock::find_upper_bound(dst_t* start, dst_t* end, dst_t value) {
  auto l = 0;
  auto r = end - start >> 1;
  while (l <= r) {  // Incorrect if not ended before r-l > 4 because
                    // there could be an endless loop.
    auto m = r + l >> 1;
    auto v = start[m << 1];
    if (value > v) {
      l = m + 1;
    } else if (v == value) {
      return start + (m << 1);
    } else {
      r = m - 1;
    }
  }
  return end;
}

void MyEdgeBlock::getReadLock() { spin_rw_lock.lock_read(); }
void MyEdgeBlock::unleashReadLock() { spin_rw_lock.unlock(); }

dst_t MyEdgeBlock::get_src() { return src_; }