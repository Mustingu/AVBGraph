#include "CDLP.h"

#include <omp.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <unordered_map>

CDLP::CDLP(GraphStore* input_graph, AllVBManager* input_vbm, int input_thread)
    : graph(input_graph), vbm(input_vbm), thread_num(input_thread) {
  max_vid = graph->get_node_num();
  num_vertices = max_vid;
  labels.resize(max_vid);
  result.resize(max_vid);
}

void CDLP::compute_cdlp(uint64_t max_iterations, GraphStore* MEA) {
  Transaction* txn = new Transaction(1, true, false, graph);
  vbm->registerROTransaction(txn);
  epoch_t read_ts = txn->get_read_epoch();

  max_vid = graph->get_node_num();
  labels.resize(max_vid);

  // Initialize: each vertex gets its own ID as label
#pragma omp parallel for
  for (uint64_t v = 0; v < max_vid; v++)
    labels[v] = v;

  std::vector<uint64_t> next_labels(max_vid);
  std::atomic<bool> changed(true);

  for (uint64_t iter = 0; iter < max_iterations && changed.load(); iter++) {
    changed.store(false);

#pragma omp parallel for schedule(dynamic, 64)
    for (uint64_t v = 0; v < max_vid; v++) {
      uint64_t deg = graph->check_degree(v, read_ts);
      if (deg == 0) { next_labels[v] = labels[v]; continue; }

      // Run-length counting on sorted neighbors (avoids hash map overhead)
      uint64_t best_label = labels[v];
      uint64_t best_count = 0;
      uint64_t run_label = ~0ull, run_count = 0;
      auto* ds = graph->GetBlockByIndex(v);
      ds->getReadLock();
      ds->for_each_edge_sorted(read_ts,
          [&](EdgeWithIndex* edge) {
            uint64_t lbl = labels[edge->e & ~DELETION_MASK];
            if (lbl == run_label) { run_count++; }
            else {
              if (run_count > best_count || (run_count == best_count && run_label < best_label))
                { best_label = run_label; best_count = run_count; }
              run_label = lbl; run_count = 1;
            }
          });
      // Flush last run
      if (run_count > best_count || (run_count == best_count && run_label < best_label))
        { best_label = run_label; best_count = run_count; }
      ds->unleashReadLock();
      if (best_label != labels[v]) {
        changed.store(true);
        next_labels[v] = best_label;
      } else {
        next_labels[v] = labels[v];
      }
    }

    std::swap(labels, next_labels);
  }

  vbm->deregisterROTransaction();
  delete txn;

#pragma omp parallel for num_threads(thread_num)
  for (uint64_t i = 0; i < max_vid; i++) {
    if (MEA != nullptr && i < MEA->get_node_num())
      result[i] = std::make_pair(MEA->p_mHashMap[i], labels[i]);
    else
      result[i] = std::make_pair(i, labels[i]);
  }
}
