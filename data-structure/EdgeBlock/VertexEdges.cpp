#include "data-structure/EdgeBlock/VertexEdges.h"

VertexEdges::VertexEdges(dst_t src, dst_t* _start, size_t _capacity,
                         size_t _edges_and_versions)
    : src_(src),
      start((EdgeWithIndex*)_start),
      edges_and_versions(_edges_and_versions),
      VBM(src) {
  for (int i = 0; i < MAXSIMULBATCH; i++) {
    tmp_vb[i] = VBM.tvb_array + i;
  }
};

VertexEdges::VertexEdges() : start(nullptr), edges_and_versions(0), VBM() {
  for (int i = 0; i < MAXSIMULBATCH; i++) {
    tmp_vb[i] = VBM.tvb_array + i;
  }
}
VertexEdges::VertexEdges(dst_t src)
    : src_(src), start(nullptr), edges_and_versions(0), VBM(src) {
  for (int i = 0; i < MAXSIMULBATCH; i++) {
    tmp_vb[i] = VBM.tvb_array + i;
  }
};

void VertexEdges::setSrc(dst_t src) {
  src_ = src;
  VBM.setSrc(src);
}
VertexEdges::VertexEdges(const VertexEdges& other)
    : start(other.start),
      edges_and_versions(other.edges_and_versions),
      VBM(other.src_) {
  for (int i = 0; i < MAXSIMULBATCH; i++) {
    tmp_vb[i] = VBM.tvb_array + i;
  }
}

unsigned VertexEdges::Transform(TmpVersionBlock* tvb, VersionBlock* vb) {
  auto n = tvb->GetVersionNum();
  if (!n) return 0;
  std::vector<size_t> indices(n);
  std::iota(indices.begin(), indices.end(), 0);

  std::sort(indices.begin(), indices.end(),
            [&tvb](size_t i, size_t j) {
              // Sort by composite (coarse_c, intra_c) ascending
              if (tvb->tmp_entry_[i].last_epoch !=
                  tvb->tmp_entry_[j].last_epoch)
                return tvb->tmp_entry_[i].last_epoch <
                       tvb->tmp_entry_[j].last_epoch;
              return tvb->tmp_entry_[i].old_intra_c <
                     tvb->tmp_entry_[j].old_intra_c;
            });
  auto epoch = vb->timestamp_;
  spin_rw_lock.lock();

  int i = 0;
  int vb_alloc = 0;  // extra VB records from delete-in-first-loop
  for (; i < n && !tvb->tmp_entry_[indices[i]].last_epoch; i++) {
    auto& tvb_item = tvb->tmp_entry_[indices[i]];

    if (is_delete(tvb_item.edge)) { vb_alloc++; continue; }  // handled below

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
#ifdef FINEGRAIN
      item_e->index = tvb_item.new_intra_c;
#else
      item_e->index = -1;
#endif
      item_p->link = nullptr;
    } else {
      item_p->link = reinterpret_cast<VersionBlock*>(
          (static_cast<uint64_t>(epoch) << 32) | (unsigned)-1);
      auto next_vb = tmp_vb[tvb_item.link.block & MODPM];
      next_vb->ChangeLastIndex(tvb_item.link.index, nullptr);
    }

    item_p->properties = tvb_item.weight;
    vb->edge_num_++;
  }

  int item_num = n - i + vb_alloc;
  if (item_num != 0)
    vb->start_ = new EdgeWithIndex[item_num << 1];
  else {
    VBM.PushVB(edge_num_, vb);
    spin_rw_lock.unlock();
    if (tvb->tmp_entry_ != nullptr) delete[] tvb->tmp_entry_;
    tvb->Clear();
#ifdef FINEGRAIN
    VBM.MarkTvbInactive(epoch);
#endif
    return 0;
  }

  int sm = 0;
  vb->version_num_ = item_num;

  i += vb_alloc;  // skip deletes handled above
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
#ifdef FINEGRAIN
    // Fine-grained VB record: read OLD values from TVB entry, not from PA
    // (PA was already updated by insert_edge_block)
    // [j]: {dst, old_coarse_c, old_intra_c}
    vb->start_[j].SetEdgeFG(tvb_item.edge & ~DELETION_MASK,
                            tvb_item.last_epoch,   // old coarse_c
                            tvb_item.old_intra_c); // old intra_c
    // [j+N]: {old_weight, old_pred_index, intra_inv = new_intra_c}
    vb->start_[j + item_num].SetPropFG(item_p->properties,
                                       tvb_item.link.index,   // old pred_index
                                       tvb_item.new_intra_c); // intra_inv

    // Update PA: new version replaces old
    item_e->block = epoch;
    item_e->index = tvb_item.new_intra_c;   // intra_c of the new version
    item_p->SetPropFG(tvb_item.weight,
                      j,                     // pred_index → this VB entry
                      0);                    // intra_inv = 0 (not yet invalidated)
#else
    // std::memcpy(&vb->start_[j].link, &item->link.link, sizeof(uint64_t));
    vb->start_[j].Set(tvb_item.edge & ~DELETION_MASK, (item_e->block & ~TMPVB_MASK),
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
#endif

    // std::swap(item_p->properties, tvb_item.weight);
  }
  // Process deletes skipped from first loop (last_epoch==0 but need VB record)
  {
    int j = vb->version_num_;
    int del_base = i;  // first skip position = first delete with last_epoch==0
    for (int d = 0; d < vb_alloc; d++, j++) {
      auto& tvb_item = tvb->tmp_entry_[indices[del_base + d]];
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
#ifdef FINEGRAIN
      vb->start_[j].SetEdgeFG(tvb_item.edge & ~DELETION_MASK, tvb_item.last_epoch, tvb_item.old_intra_c);
      vb->start_[j + item_num].SetPropFG(item_p->properties, tvb_item.link.index, tvb_item.new_intra_c);
      item_e->block = epoch;
      item_e->index = tvb_item.new_intra_c;
      item_p->SetPropFG(tvb_item.weight, j, 0);
#else
      vb->start_[j].Set(tvb_item.edge & ~DELETION_MASK, (item_e->block & ~TMPVB_MASK), item_e->index);
      vb->start_[j + item_num].Set(item_p->properties, tvb_item.link.link);
      item_e->link = item_p->link;
      item_p->Set(tvb_item.weight, vb);
#endif
      vb->edge_num_--;
      vb->version_num_++;
    }
  }
  VBM.PushVB(edge_num_, vb);
  spin_rw_lock.unlock();
  delete[] tvb->tmp_entry_;
  tvb->Clear();
#ifdef FINEGRAIN
  VBM.MarkTvbInactive(epoch);
#endif
  return vb->version_num_;
}

void VertexEdges::merge_tmpev_with_eb() {
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
bool VertexEdges::get_edge_index2(dst_t e, EdgeWithIndex*& item_e,
                                  EdgeWithIndex*& item_p) {
  int l = 0, r = edges_and_versions;
  while (l <= r) {
    int m = (l + r) >> 1;
    dst_t key = start[m].e & ~DELETION_MASK;  // strip DELETION_MASK for comparison
    if (key > e) {
      r = m - 1;
    } else if (key < e) {
      l = m + 1;
    } else {
      item_e = start + m;
      item_p = item_e + capacity;
      return true;
    }
  }
  return false;
}
bool VertexEdges::get_edge_index(dst_t e, EdgeWithIndex*& item_e,
                                 EdgeWithIndex*& item_p, unsigned& offset) {
  int l = 0, r = edges_and_versions;

  if (r != 0)
    while (l <= r) {
      int m = (l + r) >> 1;
      dst_t key = start[m].e & ~DELETION_MASK;
      if (key > e) {
        r = m - 1;
      } else if (key < e) {
        l = m + 1;
      } else {
        item_e = start + m;
        item_p = item_e + capacity;
        offset = m;
        return true;
      }
    }

  for (int i = 0; i < tmp_ev; i++) {
    if ((tmp_item[i].e & ~DELETION_MASK) == e) {
      item_e = tmp_item + i;
      item_p = item_e + TMPNUM;
      offset = i | TMPVB_MASK;
      return true;
    }
  }

  return false;
}
bool VertexEdges::insert_edge_block(dst_t* e, epoch_t epoch, Transaction* txn,
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
    item_e->e &= ~DELETION_MASK;  // clear stale deletion flag on re-insert
  }

  if (vb->timestamp_ != epoch) {
    txn->write_lock_pushback(vb);
    vb->timestamp_ = epoch;
    vb->version_num_ = 0;
#ifdef FINEGRAIN
    VBM.MarkTvbActive(epoch);
#endif
  }

  bool is_tmp = (item_e->block & TMPVB_MASK);
#ifdef FINEGRAIN
  // Fine-grained: unified read of old state
  //   [0].block & ~TMPVB_MASK = coarse_c, [0].index = intra_c
  //   [1].block = pred_index (or TVB entry index if tmp), [1].index = intra_inv
  epoch_t last_epoch = item_e->GetCoarseC();
  unsigned last_index = item_p->block;
  intra_t old_intra_c = item_e->index;
#else
  epoch_t last_epoch = is_tmp ? item_p->block : item_e->block,
          last_index = is_tmp ? item_p->index : item_e->index;
#endif

  /* this RW-txn has intermedia writes during read and write epoch, which is
   illegal OR old_txn is later on timestamps layer*/
  if (!txn->is_write_only() && txn->get_read_epoch() < last_epoch ||
      last_epoch > epoch) {
    spin_rw_lock.unlock();
    return false;
  }
#ifdef FINEGRAIN
  // Fine-grained: always InsertVersion (never ChangeVersion).
  // Same-epoch same-edge writes produce separate TVB entries.
  {
    intra_t intra_c = txn->get_intra_c();
    unsigned tvb_idx = vb->InsertVersion(e, last_epoch, last_index,
        reinterpret_cast<VersionBlock*>(static_cast<uint64_t>(last_epoch) << 32 | last_index),
        txn, offset, merge_times, last_epoch, old_intra_c, intra_c);

    if (is_tmp && last_epoch != 0)
      tmp_vb[epoch & MODPM]->ChangeNextIndex(last_index, epoch, tvb_idx);

    // Update PA with fine-grained layout
    // Store current epoch in block; old coarse_c is preserved in TVB entry
    item_e->block = epoch | TMPVB_MASK;
    item_e->index = intra_c;           // intra_c in [0]
    item_p->block = tvb_idx;           // TVB entry index in [1].block (temporary)
    item_p->index = 0;                 // intra_inv = 0 (not yet invalidated)
  }
#else
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
#endif

#ifdef FINEGRAIN
  txn->AddEB(this);
#else
  spin_rw_lock.unlock();
#endif

  return true;
};

bool VertexEdges::delete_edge_block(dst_t* e, epoch_t epoch, Transaction* txn,
                                    bool new_entry) {
  TmpVersionBlock* vb = tmp_vb[epoch & MODPM];
  spin_rw_lock.lock();

  if (tmp_ev == TMPNUM) merge_tmpev_with_eb();

  uint64_t link;
  EdgeWithIndex *item_e, *item_p;
  auto edge = *e;
  unsigned offset = 0;

  // Edge must exist for deletion
  if (!M.getDestHashTableVal(edge, link)) {
    spin_rw_lock.unlock();
    return false;
  }
  get_edge_index(edge, item_e, item_p, offset);

  if (vb->timestamp_ != epoch) {
    txn->write_lock_pushback(vb);
    vb->timestamp_ = epoch;
    vb->version_num_ = 0;
#ifdef FINEGRAIN
    VBM.MarkTvbActive(epoch);
#endif
  }

  bool is_tmp = (item_e->block & TMPVB_MASK);
#ifdef FINEGRAIN
  epoch_t last_epoch = item_e->GetCoarseC();
  unsigned last_index = item_p->block;
  intra_t old_intra_c = item_e->index;
#else
  epoch_t last_epoch = is_tmp ? item_p->block : item_e->block,
          last_index = is_tmp ? item_p->index : item_e->index;
#endif

  if ((!txn->is_write_only() && txn->get_read_epoch() < last_epoch) ||
      last_epoch > epoch) {
    spin_rw_lock.unlock();
    return false;
  }

#ifdef FINEGRAIN
  {
    intra_t intra_c = txn->get_intra_c();
    unsigned tvb_idx = vb->InsertVersion(
        e, last_epoch, last_index,
        reinterpret_cast<VersionBlock*>(static_cast<uint64_t>(last_epoch) << 32 | last_index),
        txn, offset, merge_times, last_epoch, old_intra_c, intra_c);
    if (is_tmp && last_epoch != 0)
      tmp_vb[epoch & MODPM]->ChangeNextIndex(last_index, epoch, tvb_idx);
    item_e->e |= DELETION_MASK;
    item_e->block = epoch | TMPVB_MASK;
    item_e->index = intra_c;
    item_p->block = tvb_idx;
    item_p->index = 0;
  }
  txn->AddEB(this);
#else
  if (last_epoch == epoch) {
    if (!vb->ChangeVersion(e, last_index, txn, merge_times, offset)) {
      spin_rw_lock.unlock();
      return false;
    }
    item_e->e |= DELETION_MASK;
  } else {
    if (is_tmp) {
      item_p->index = vb->InsertVersion(
          e, TMPVB_MASK, 0,
          reinterpret_cast<VersionBlock*>(
              (static_cast<uint64_t>(last_epoch) << 32) | last_index, epoch),
          txn, offset, merge_times, last_epoch);
      if (last_epoch != 0)
        tmp_vb[last_epoch & MODPM]->ChangeNextIndex(last_index, epoch, item_p->index);
    } else {
      auto last_vb = item_p->link;
      item_p->index = vb->InsertVersion(e, 0, 0, last_vb, txn, offset,
                                        merge_times, last_epoch);
    }
    item_e->e |= DELETION_MASK;
    item_e->block |= TMPVB_MASK;
    item_p->block = epoch;
  }
  spin_rw_lock.unlock();
#endif

  return true;
}

void VertexEdges::build(unsigned _num, dst_t* _edges, dst_t* _properties) {
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
    M.setDestHashTableVal(_edges[i], i);  // register in hash table
  }

  for (int i = 0; i < MAXSIMULBATCH; i++) {
    tmp_vb[i]->SetEdgeBlock(this);
  }

  tmp_item = reinterpret_cast<EdgeWithIndex*>(
      la->Allocate(sizeof(EdgeWithIndex) * TMPNUM * 2, threadid));
  // tmp_item = reinterpret_cast<EdgeWithIndex*>(
  //     std::malloc(sizeof(EdgeWithIndex) * TMPNUM * 2));
}

void VertexEdges::my_print_block() {}

unsigned VertexEdges::get_degree(epoch_t epoch) {
  return (edge_num_ + VBM.getDegreeVersioned(epoch));
}

// NOTE: build this function for test
double VertexEdges::getSum(
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
    if (!is_delete(i->e) && i->IsVisibleAt(epoch)) {
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

dst_t* VertexEdges::find_upper_bound(dst_t* start, dst_t* end, dst_t value) {
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

void VertexEdges::getReadLock() { spin_rw_lock.lock_read(); }
void VertexEdges::unleashReadLock() { spin_rw_lock.unlock(); }

dst_t VertexEdges::get_src() { return src_; }