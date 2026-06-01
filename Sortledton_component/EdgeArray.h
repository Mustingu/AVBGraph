
#ifndef LIVE_GRAPH_TWO_VERSIONINGBLOCKEDSKIPLISTADJACENCYLIST_H
#define LIVE_GRAPH_TWO_VERSIONINGBLOCKEDSKIPLISTADJACENCYLIST_H

#include <atomic>
#include <forward_list>
#include <mutex>
#include <random>
#include <vector>

#include "EdgeBlock.h"
#include "TransactionManager.h"
#include "VersionedTopologyInterface.h"

class EdgeArray : public STL_VersionedTopologyInterface {
 public:
  TransactionManager& tm;
  std::vector<EdgeBlock*> blocks;
  size_t block_count;
  size_t property_size_;

  EdgeArray(size_t block_count, size_t property_size, TransactionManager& tm)
      : block_count(block_count), property_size_(property_size), tm(tm) {
    blocks.resize(block_count, nullptr);
  }

  ~EdgeArray() {
    for (size_t i = 0; i < block_count; i++) {
      delete blocks[i];
    }
  }

  vertex_id_t logical_id(vertex_id_t id) { return id; }
  vertex_id_t physical_id(vertex_id_t id) { return id; }

  size_t vertex_count_version(version_t version) {  // Not Implemented
    return 0;
  };
  size_t max_physical_vertex() { return block_count; }
  size_t edge_count_version(version_t version) {  // Not Implemented
    return 0;
  };

  bool has_vertex_version(vertex_id_t v, version_t version) {
    return v < block_count;
  }
  bool has_vertex_version_p(vertex_id_t v, version_t version) {
    return v < block_count;
  }
  bool insert_vertex_version(vertex_id_t v,
                             version_t version) {  // Not Implemented
    return false;
  };
  bool delete_vertex_version(vertex_id_t v,
                             version_t version) {  // Not Implemented
    return false;
  };
  bool has_edge_version(edge_t edge,
                        version_t version) {  // Not Implemented
    return false;
  };
  bool has_edge_version_p(edge_t edge,
                          version_t version) {  // Not Implemented
    return false;
  };

  bool get_weight_version(edge_t edge, version_t version,
                          char* out) {  // Not Implemented
    return false;
  };
  bool get_weight_version_p(edge_t edge, version_t version,
                            char* out) {  // Not Implemented
    return false;
  };

  size_t neighbourhood_size_version(vertex_id_t src,
                                    version_t version) {  // Not Implemented
    return -1;
  };
  size_t neighbourhood_size_version_p(vertex_id_t src,
                                      version_t version) {  // Not Implemented
    return -1;
  };

  bool insert_edge_version(edge_t edge,
                           version_t version) {  // Not Implemented
    return false;
  }
  bool insert_edge_version(edge_t edge, version_t version, char* properties,
                           size_t properties_size) {
    blocks[edge.src]->gc(tm.getMinActiveVersion(), tm.get_sorted_versions());
    return blocks[edge.src]->insert_edge(edge.dst, version, properties);
  }
  bool delete_edge_version(edge_t edge,
                           version_t version) {  // Not Implemented
    return false;
  }
  bool aquire_vertex_lock(vertex_id_t vertex_lock) {
    return blocks[vertex_lock]->aquire_vertex_lock(), true;
  }
  void release_vertex_lock(vertex_id_t v) { blocks[v]->release_vertex_lock(); }
  void aquire_vertex_lock_p(vertex_id_t vertex_lock) {
    blocks[vertex_lock]->aquire_vertex_lock();
  }
  void release_vertex_lock_p(vertex_id_t v) {
    blocks[v]->release_vertex_lock();
  }
  void aquire_vertex_lock_shared_p(vertex_id_t vertex_lock) {
    blocks[vertex_lock]->aquire_vertex_lock_shared();
  }
  void release_vertex_lock_shared_p(vertex_id_t v) {
    blocks[v]->release_vertex_lock_shared();
  }

  void report_storage_size() {};

  void bulkload(const SortedCSRDataSource& src) {};

  void gc_all() {};
  void gc_vertex(vertex_id_t v) {};

  void rollback_vertex_insert(vertex_id_t v) {};
  void SetBlockByIndex(unsigned index, unsigned _num, dst_t* _edges,
                       char* _properties) {
    blocks[index] = new EdgeBlock(index, property_size_);
    blocks[index]->build(_num, _edges, _properties);
  }

  void SetBlockByIndex(unsigned index, EdgeBlock* eb) { blocks[index] = eb; }
  EdgeBlock* GetBlockByIndex(unsigned index) const { return blocks[index]; }
};

#endif  // LIVE_GRAPH_TWO_VERSIONINGBLOCKEDSKIPLISTADJACENCYLIST_H