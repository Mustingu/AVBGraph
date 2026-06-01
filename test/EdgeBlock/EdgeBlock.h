#ifndef EDGE_BLOCK_H
#define EDGE_BLOCK_H

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

class EdgeBlock {
 public:
  EdgeBlock() = delete;
  EdgeBlock(const EdgeBlock&) = delete;
  EdgeBlock& operator=(const EdgeBlock&) = delete;

  EdgeBlock(dst_t* start, size_t capacity, size_t edges_and_versions,
            size_t properties, size_t property_size)
      : eb_(start, capacity, edges_and_versions, properties, property_size) {}

  ~EdgeBlock() {
    if (eb_.has_space_to_insert_edge()) {
      eb_.insert_edge(0, 0);
    }
    if (eb_.has_space_to_insert_edge()) {
      eb_.insert_edge(0, 0);
    }
  }

  dst_t get_min_edge() { return eb_.get_min_edge(); }

  dst_t get_max_edge() { return eb_.get_max_edge(); }

  size_t get_edges_and_versions() { return eb_.get_edges_and_versions(); }

  dst_t* get_single_block_pointer() { return eb_.get_single_block_pointer(); }

  bool has_space_to_insert_edge() { return eb_.has_space_to_insert_edge(); }

  bool has_space_to_delete_edge() { return eb_.has_space_to_delete_edge(); }

  void insert_edge(dst_t edge, uint64_t version) {
    if (eb_.has_space_to_insert_edge()) {
      eb_.insert_edge(edge, version);
    }
  }

  void delete_edge(dst_t edge) {
    if (eb_.has_space_to_delete_edge()) {
      eb_.delete_edge(edge);
    }
  }

 private:
  MyEdgeBlock eb_;
};

#endif  // EDGE_BLOCK_H
