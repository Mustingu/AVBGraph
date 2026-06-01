#include "MyEdgeArray.h"

MEBC::MEBC() { eb = nullptr; }
MEBC::MEBC(MyEdgeBlock* eb) : eb(eb) {}

MyEdgeArray::MyEdgeArray(size_t block_count) : block_count(block_count) {
  p_mHashMap.resize(block_count);
  blocks.resize(block_count);
}

bool MyEdgeArray::insert_edge_block(dst_t* edge, epoch_t epoch,
                                    Transaction* txn) {
  auto v = edge[0], e = edge[1];
  bool it = 1;
  bool f = blocks[v].eb->insert_edge_block(edge + 1, epoch, txn, !it);
  return f;
}

template <typename T>
void MyEdgeArray::grow_vector_if_smaller(tbb::concurrent_vector<T>& v, size_t s,
                                         T init_value) {
  if (v.capacity() <=
      s) {  // Only synchronize with other threads if potentially necessary
    std::scoped_lock<std::mutex> l(growing_vector_mutex);
    if (v.capacity() <= s) {
      v.grow_to_at_least(v.capacity() * 2, init_value);
    }
  }
}
vertex_id_t MyEdgeArray::new_vertex() {
  auto eb = new MyEdgeBlock();
  eb->build(0, nullptr, nullptr);
  auto v = node_num.fetch_add(1);
  eb->setSrc(v);
  if (blocks.capacity() <= v) {
    std::scoped_lock<std::mutex> lock(growing_vector_mutex);
    if (blocks.capacity() <= v) {
      blocks.grow_to_at_least(blocks.capacity() * 2, MEBC());
    }
  }
  blocks[v].eb = eb;

  return v;
}
unsigned MyEdgeArray::check_degree(unsigned index) {
  return blocks[index].eb->get_degree();
}
unsigned MyEdgeArray::check_degree(unsigned index, epoch_t epoch) {
  blocks[index].eb->getReadLock();
  auto degree = blocks[index].eb->get_degree(epoch);
  blocks[index].eb->unleashReadLock();
  return degree;
}
