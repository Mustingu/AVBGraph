#include "BFS.h"

BFS::BFS(MyEdgeArray* input_graph, AllVBManager* input_vbm, int input_thread)
    : graph(input_graph), vbm(input_vbm), thread_num(input_thread) {
  max_vid = graph->get_node_num();
  num_vertices = max_vid;
  distances.resize(max_vid);
  result.resize(max_vid);
}

int64_t BFS::init_distance(Transaction& txn) {
  uint64_t total_edge_num(0);
#pragma omp parallel for reduction(+ : total_edge_num)
  for (uint64_t n = 0; n < max_vid; n++) {
    uint64_t out_degree = graph->check_degree(n, txn.get_read_epoch());
    if (out_degree == std::numeric_limits<int64_t>::max()) [[unlikely]] {
      distances[n] = out_degree;
    } else [[likely]] {
      total_edge_num += out_degree;
      distances[n] = out_degree != 0 ? -out_degree : -1;
    }
  }
  return total_edge_num / 2;
}

void BFS::bfs(uint64_t root, int alpha, int beta, MyEdgeArray* MEA) {
  Transaction* txn = new Transaction(1, true, false, graph);
  vbm->registerROTransaction(txn);
  auto read_ts = txn->get_read_epoch();

  if (max_vid != graph->get_node_num()) [[unlikely]] {
    max_vid = graph->get_node_num();
    distances.resize(max_vid);
    result.resize(max_vid);
  }

  gapbs::SlidingQueue<int64_t> queue(max_vid);
  queue.push_back(root);
  queue.slide_window();
  gapbs::Bitmap curr(max_vid);
  curr.reset();
  gapbs::Bitmap front(max_vid);
  front.reset();
  int64_t edges_to_check = init_distance(*txn);

  int64_t scout_count = 0;
  scout_count = graph->check_degree(root, read_ts);
  int64_t distance = 1;
  distances[root] = 0;

  while (!queue.empty()) {
    // std::cout << scout_count << " " << edges_to_check << " " << alpha << " "
    //           << beta << '\n';
    if (scout_count > edges_to_check / alpha) {
      // std::cout << "do_bfs_BUStep\n";
      int64_t awake_count, old_awake_count;
      // Queue to Bitmap conversion
#pragma omp parallel for
      for (auto q_iter = queue.begin(); q_iter < queue.end(); q_iter++) {
        int64_t u = *q_iter;
        front.set_bit_atomic(u);
      }

      awake_count = queue.size();
      queue.slide_window();
      do {
        // std::cout << "do_bfs_BUStep\n";
        old_awake_count = awake_count;
        awake_count = do_bfs_BUStep(*txn, distance, front, curr);
        front.swap(curr);
        distance++;
      } while ((awake_count >= old_awake_count) ||
               (awake_count > (int64_t)num_vertices / beta));

#pragma omp parallel
      {
        gapbs::QueueBuffer<int64_t> lqueue(queue);
#pragma omp for schedule(dynamic, 64)
        for (uint64_t n = 0; n < max_vid; n++)
          if (front.get_bit(n)) lqueue.push_back(n);
        lqueue.flush();
      }
      queue.slide_window();
      scout_count = 1;
    } else {
      // std::cout << "do_bfs_TDStep\n";
      edges_to_check -= scout_count;
      scout_count = do_bfs_TDStep(*txn, distance, queue);
      queue.slide_window();
      distance++;
    }
  }

  // Prepare results
#pragma omp parallel for
  for (uint64_t logical_id = 0; logical_id < max_vid; logical_id++) {
    if (MEA != nullptr && logical_id < MEA->get_node_num()) {
      result[logical_id] =
          std::make_pair(MEA->p_mHashMap[logical_id], distances[logical_id]);
    } else {
      if (distances[logical_id] == std::numeric_limits<int64_t>::max()) {
        result[logical_id] =
            std::make_pair(std::numeric_limits<uint64_t>::max(),
                           std::numeric_limits<int64_t>::max());
      } else {
        result[logical_id] = std::make_pair(logical_id, distances[logical_id]);
      }
    }
  }

  vbm->deregisterROTransaction();

  // std::cout << root << " " << alpha << " " << beta << " " << MEA << '\n';

  if (MEA != nullptr) {
    std::cout << "output bfs\n";
    std::vector<std::tuple<uint64_t, int64_t>> sorted_result(max_vid);
#pragma omp parallel for
    for (uint64_t logical_id = 0; logical_id < max_vid; logical_id++) {
      sorted_result[logical_id] =
          std::make_tuple(MEA->p_mHashMap[logical_id], distances[logical_id]);
    }
    std::sort(sorted_result.begin(), sorted_result.end(),
              [](const auto& a, const auto& b) {
                return std::get<0>(a) < std::get<0>(b);
              });
    std::ofstream output_file("bfs.output");
    if (!output_file.is_open()) {
      std::cerr << "Failed to open output file" << std::endl;
      return;
    }
    for (const auto& [vid, dist] : sorted_result) {
      output_file << vid << " " << dist << std::endl;
    }
    output_file.close();
  }
  // #pragma omp parallel for
  //   for (uint64_t logical_id = 0; logical_id < max_vid; logical_id++) {
  //     std::string_view payload = txn.get_vertex(
  //         logical_id, thread_id);  // they store external vid in the vertex
  //                                  // data for experiments
  //     if (payload.empty()) [[unlikely]] {  // the vertex does not exist
  //       result[logical_id - 1] =
  //           std::make_pair(std::numeric_limits<uint64_t>::max(),
  //                          std::numeric_limits<double>::max());
  //     } else {
  //       /*if(*(reinterpret_cast<const uint64_t*>(payload.data()))==8196461){
  //           std::cout<<" max vid is "<<max_vid<<" iteration is "<<
  //       logical_id<<std::endl; std::cout<< *(reinterpret_cast<const
  //       uint64_t*>(payload.data())) <<" "<< scores[logical_id-1]<<
  //       std::endl;
  //       }*/
  //       result[logical_id - 1] =
  //           std::make_pair(*(reinterpret_cast<const
  //           uint64_t*>(payload.data())),
  //                          scores[logical_id - 1]);
  //     }
  //   }
  delete txn;
}

int64_t BFS::do_bfs_TDStep(Transaction& txn, int64_t distance,
                           gapbs::SlidingQueue<int64_t>& queue) {
  int64_t scout_count = 0;
#pragma omp parallel reduction(+ : scout_count)
  {
    gapbs::QueueBuffer<int64_t> lqueue(queue);
#pragma omp for schedule(dynamic, 64)
    for (auto q_iter = queue.begin(); q_iter < queue.end(); q_iter++) {
      int64_t u = *q_iter;

      auto ds = graph->GetBlockByIndex(u);
      ds->getReadLock();

      GraphAlgorithms::for_each_edge(
          ds, txn.get_read_epoch(),
          [&](EdgeWithIndex* edge) {
            auto dst = edge->e & ~DELETION_MASK;
            // std::cout << "dst " << dst << '\n';
            int64_t curr_val = distances[dst];
            if (curr_val < 0 &&
                gapbs::compare_and_swap(distances[dst], curr_val, distance)) {
              // Add to local queue buffer
              lqueue.push_back(dst);
              // Note: This requires a thread-local queue or synchronization
              scout_count += -curr_val;
            }
          },
          nullptr);

      ds->unleashReadLock();
    }
    lqueue.flush();
  }
  return scout_count;
}

int64_t BFS::do_bfs_BUStep(Transaction& txn, int64_t distance,
                           gapbs::Bitmap& front, gapbs::Bitmap& next) {
  int64_t awake_count = 0;
  next.reset();

#pragma omp parallel for reduction(+ : awake_count)
  for (uint64_t u = 0; u < max_vid; u++) {
    // std::cout << "do" + std::to_string(u) + '\n';
    if (distances[u] == std::numeric_limits<int64_t>::max())
      continue;              // the vertex does not exist
    if (distances[u] < 0) {  // the node has not been visited yet
      auto ds = graph->GetBlockByIndex(u);
      ds->getReadLock();

      bool found_neighbor = false;
      GraphAlgorithms::for_each_edge_condition(
          ds, txn.get_read_epoch(), [&](EdgeWithIndex* edge) -> bool {
            if (front.get_bit(edge->e & ~DELETION_MASK)) {
              distances[u] = distance;
              awake_count++;
              next.set_bit(u);
              // std::cout << "setu " + std::to_string(edge->e & ~DELETION_MASK)
              // +
              //                  " " + std::to_string(u) + '\n';
              return true;
            }
            return false;
          });

      ds->unleashReadLock();
    }
  }
  return awake_count;
}