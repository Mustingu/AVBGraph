#include "PageRank.h"

PageRank::PageRank(MyEdgeArray* input_graph, AllVBManager* input_vbm,
                   int input_thread)
    : graph(input_graph), vbm(input_vbm), thread_num(input_thread) {
  max_vid = graph->get_node_num();
  num_vertices = max_vid;
  scores.resize(max_vid);
  result.resize(max_vid);
}

void PageRank::compute_pagerank(uint64_t num_iterations, double damping_factor,
                                MyEdgeArray* MEA, Satistical* global_counter) {
  Transaction* txn = new Transaction(1, true, false, graph);
  vbm->registerROTransaction(txn);
  auto read_ts = txn->get_read_epoch();
  max_vid = graph->get_node_num();

  scores.resize(max_vid);
  result.resize(max_vid);
  // }

  const double init_score = 1.0 / num_vertices;
  const double base_score = (1.0 - damping_factor) / num_vertices;

  std::vector<uint64_t> degrees(max_vid);

#pragma omp parallel for
  for (uint64_t v = 0; v < max_vid; v++) {
    scores[v] = init_score;
    degrees[v] = graph->check_degree(v, read_ts);
  }
  std::vector<gapbs::pvector<double>> outgoing_contrib;
  outgoing_contrib.reserve(4);
  for (int i = 0; i < 4; i++) {
    outgoing_contrib.emplace_back(max_vid, 0.0);
  }

  for (uint64_t iteration = 0; iteration < num_iterations; iteration++) {
    double dangling_sum = 0.0;

#pragma omp parallel for reduction(+ : dangling_sum)
    for (uint64_t v = 0; v < max_vid; v++) {
      uint64_t out_degree = degrees[v];
      if (out_degree == 0) {  // this is a sink
        dangling_sum += scores[v];
      } else {
        outgoing_contrib[0][v] = scores[v] / out_degree;
      }
    }
    dangling_sum /= num_vertices;

#pragma omp parallel
    {
      // 2. 每个线程拥有独立的局部副本
      thread_local Satistical local_statistic;
      local_statistic.reset();
      Satistical* nw = global_counter ? &local_statistic : nullptr;

#pragma omp for schedule(dynamic, 64)
      for (uint64_t v = 0; v < max_vid; v++) {
        double incoming_total = 0;
        if (degrees[v]) {
          auto x = outgoing_contrib[0][v];
          auto ds = graph->GetBlockByIndex(v);

          ds->getReadLock();
          GraphAlgorithms::for_each_edge(
              ds, read_ts,
              [&](EdgeWithIndex* edge) {
                incoming_total += outgoing_contrib[0][edge->e & ~DELETION_MASK];
                // incoming_total += edge->e & ~DELETION_MASK;
                return false;
              },
              nw);
          ds->unleashReadLock();
        }
        scores[v] =
            base_score + damping_factor * (incoming_total + dangling_sum);
      }
#pragma omp critical
      {
        if (global_counter) {
          global_counter->record_cnt += local_statistic.record_cnt;
          global_counter->visible_record_cnt +=
              local_statistic.visible_record_cnt;
        }
      }
    }
  }

#pragma omp parallel for num_threads(thread_num)
  for (uint64_t logical_id = 0; logical_id < max_vid; logical_id++) {
    if (MEA != nullptr && logical_id < MEA->get_node_num()) {
      result[logical_id] =
          std::make_pair(MEA->p_mHashMap[logical_id], scores[logical_id]);
    } else {
      if (scores[logical_id] == std::numeric_limits<int64_t>::max()) {
        result[logical_id] =
            std::make_pair(std::numeric_limits<uint64_t>::max(),
                           std::numeric_limits<int64_t>::max());
      } else {
        result[logical_id] = std::make_pair(logical_id, scores[logical_id]);
      }
    }
  }

  vbm->deregisterROTransaction();

  if (MEA != nullptr) {
    std::ofstream outfile("output_pr.result");

    // 创建一个索引向量用于排序
    std::vector<uint64_t> indices(max_vid);
    std::iota(indices.begin(), indices.end(), 0);

    // 按照 MEA->p_mHashMap[v] 的值对索引进行排序
    std::sort(indices.begin(), indices.end(), [&](uint64_t a, uint64_t b) {
      return MEA->p_mHashMap[a] < MEA->p_mHashMap[b];
    });

    // 按照排序后的顺序输出到文件
    for (uint64_t i = 0; i < max_vid; i++) {
      uint64_t v = indices[i];
      outfile << MEA->p_mHashMap[v] << " " << scores[v] << std::endl;
    }
    outfile.close();
  }
}