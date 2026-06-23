#include "LCC.h"

#include <omp.h>

#include <algorithm>
#include <cstdio>

LCC::LCC(GraphStore* input_graph, AllVBManager* input_vbm, int input_thread)
    : graph(input_graph), vbm(input_vbm), thread_num(input_thread) {
  max_vid = graph->get_node_num();
  num_vertices = max_vid;
  lcc_scores.resize(max_vid, 0.0);
  result.resize(max_vid);
}

void LCC::compute_lcc(GraphStore* MEA) {
  Transaction* txn = new Transaction(1, true, false, graph);
  vbm->registerROTransaction(txn);
  epoch_t read_ts = txn->get_read_epoch();

  max_vid = graph->get_node_num();
  lcc_scores.resize(max_vid, 0.0);

#pragma omp parallel for schedule(dynamic, 64)
  for (uint64_t v = 0; v < max_vid; v++) {
    uint64_t deg = graph->check_degree(v, read_ts);
    if (deg < 2) continue;

    // Collect out-neighbors sorted
    std::vector<uint64_t> neighbors;
    neighbors.reserve(deg);
    auto* ds = graph->GetBlockByIndex(v);
    ds->getReadLock();
    GraphAlgorithms::for_each_edge(ds, Composite(read_ts, 0),
        [&](EdgeWithIndex* edge) {
          neighbors.push_back(edge->e & ~DELETION_MASK);
        }, nullptr);
    ds->unleashReadLock();
    std::sort(neighbors.begin(), neighbors.end());

    // Count triangles: binary search in v's sorted neighbor set
    uint64_t triangles = 0;
    for (uint64_t u : neighbors) {
      auto* ds_u = graph->GetBlockByIndex(u);
      ds_u->getReadLock();
      GraphAlgorithms::for_each_edge(ds_u, Composite(read_ts, 0),
          [&](EdgeWithIndex* edge) {
            uint64_t w = edge->e & ~DELETION_MASK;
            if (w != v && std::binary_search(neighbors.begin(), neighbors.end(), w))
              triangles++;
          }, nullptr);
      ds_u->unleashReadLock();
    }

    lcc_scores[v] = (double)(2 * triangles) / (deg * (deg - 1));
  }

  vbm->deregisterROTransaction();
  delete txn;

#pragma omp parallel for num_threads(thread_num)
  for (uint64_t i = 0; i < max_vid; i++) {
    if (MEA != nullptr && i < MEA->get_node_num())
      result[i] = std::make_pair(MEA->p_mHashMap[i], lcc_scores[i]);
    else
      result[i] = std::make_pair(i, lcc_scores[i]);
  }
}
