#ifndef CSR_BASELINE_H
#define CSR_BASELINE_H

#include <omp.h>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <iostream>
#include <map>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "gapbs.h"  // 复用原有系统的 gapbs 工具

class CSRGraph {
 public:
  // CSR 核心数据结构
  std::vector<uint64_t> row_ptr;
  std::vector<uint64_t> col_ind;
  std::vector<double> weights;

  // ID 映射 (Sparse <-> Dense)
  std::unordered_map<uint64_t, uint64_t> raw_to_dense;

  std::vector<uint64_t> dense_to_raw;

  uint64_t num_nodes;
  uint64_t num_edges;

  CSRGraph() : num_nodes(0), num_edges(0) {}

  // 从 update_tuple 构建 CSR
  void build(
      const std::vector<std::tuple<unsigned, unsigned, uint64_t>>& raw_edges) {
    raw_to_dense.clear();
    dense_to_raw.clear();
    row_ptr.clear();
    col_ind.clear();
    weights.clear();

    num_edges = raw_edges.size();

    // 先预留空间，减少 rehash / realloc
    raw_to_dense.reserve(raw_edges.size() * 2);
    dense_to_raw.reserve(raw_edges.size() * 2);

    auto get_or_add_dense = [&](uint64_t raw_id) -> uint64_t {
      auto it = raw_to_dense.find(raw_id);
      if (it != raw_to_dense.end()) return it->second;

      uint64_t dense_id = dense_to_raw.size();
      raw_to_dense.emplace(raw_id, dense_id);
      dense_to_raw.push_back(raw_id);
      return dense_id;
    };

    struct DenseEdge {
      uint64_t u;
      uint64_t v;
      double w;
    };

    // 第一步：先把 raw edge 转成 dense edge
    // 这一步先单线程做，原因是要往 raw_to_dense 里插入新点
    std::vector<DenseEdge> dense_edges(num_edges);
    for (size_t i = 0; i < raw_edges.size(); ++i) {
      const auto& edge = raw_edges[i];
      uint64_t u = get_or_add_dense(std::get<0>(edge));
      uint64_t v = get_or_add_dense(std::get<1>(edge));

      union {
        uint64_t int_val;
        double dbl_val;
      } converter;
      converter.int_val = std::get<2>(edge);

      dense_edges[i] = {u, v, converter.dbl_val};
    }

    num_nodes = dense_to_raw.size();

    // 第二步：并行统计 degree
    std::vector<uint64_t> degrees(num_nodes, 0);

#pragma omp parallel for schedule(static)
    for (size_t i = 0; i < num_edges; ++i) {
      uint64_t u = dense_edges[i].u;
#pragma omp atomic
      degrees[u]++;
    }

    // 第三步：前缀和
    row_ptr.resize(num_nodes + 1);
    row_ptr[0] = 0;
    for (uint64_t i = 0; i < num_nodes; ++i) {
      row_ptr[i + 1] = row_ptr[i] + degrees[i];
    }

    // 第四步：并行填 CSR
    col_ind.resize(num_edges);
    weights.resize(num_edges);

    std::vector<uint64_t> offsets = row_ptr;

#pragma omp parallel for schedule(static)
    for (size_t i = 0; i < num_edges; ++i) {
      const auto& e = dense_edges[i];

      uint64_t idx;
#pragma omp atomic capture
      idx = offsets[e.u]++;

      col_ind[idx] = e.v;
      weights[idx] = e.w;
    }

    std::cout << "[CSR Build] Dense Nodes: " << num_nodes
              << ", Edges: " << num_edges << std::endl;
  }

  inline uint64_t out_degree(uint64_t u) const {
    return row_ptr[u + 1] - row_ptr[u];
  }
};

class CSRBaseline {
 public:
  CSRGraph* graph;
  std::vector<int64_t> bfs_dist;
  std::vector<double> sssp_dist;
  std::vector<double> pr_scores;

  CSRBaseline(CSRGraph* g) : graph(g) {}

  // ==========================================================
  // BFS: Direction Optimizing (严格对应 BFS.cpp)
  // ==========================================================
  void bfs(uint64_t raw_root, int alpha = 15, int beta = 18) {
    if (graph->raw_to_dense.find(raw_root) == graph->raw_to_dense.end()) {
      std::cerr << "Root node not found in graph." << std::endl;
      return;
    }
    uint64_t root = graph->raw_to_dense[raw_root];
    uint64_t num_vertices = graph->num_nodes;

    bfs_dist.assign(num_vertices, -1);  // 对应 distances

    gapbs::SlidingQueue<int64_t> queue(num_vertices);
    queue.push_back(root);
    queue.slide_window();

    gapbs::Bitmap curr(num_vertices);
    curr.reset();
    gapbs::Bitmap front(num_vertices);
    front.reset();

    uint64_t total_edge_num(0);
#pragma omp parallel for reduction(+ : total_edge_num)
    for (uint64_t n = 0; n < num_vertices; n++) {
      uint64_t out_degree = graph->out_degree(n);
      // uint64_t out_degree = 1;
      total_edge_num += out_degree;
      bfs_dist[n] = out_degree != 0 ? -out_degree : -1;
    }
    // return;

    bfs_dist[root] = 0;

    // 模拟 init_distance 逻辑：计算总边数作为 edges_to_check
    int64_t edges_to_check = total_edge_num / 2;
    std::cout << edges_to_check << std::endl;
    int64_t scout_count = graph->out_degree(root);
    int64_t distance = 1;

    while (!queue.empty()) {
      if (scout_count > edges_to_check / alpha) {
        // === Bottom-Up Step ===
        int64_t awake_count, old_awake_count;

// Queue to Bitmap
#pragma omp parallel for
        for (auto q_iter = queue.begin(); q_iter < queue.end(); q_iter++) {
          front.set_bit_atomic(*q_iter);
        }

        awake_count = queue.size();
        queue.slide_window();

        do {
          old_awake_count = awake_count;
          awake_count = 0;
          curr.reset();

// 遍历所有顶点 (严格对应 do_bfs_BUStep)
#pragma omp parallel for reduction(+ : awake_count)
          for (uint64_t u = 0; u < num_vertices; u++) {
            if (bfs_dist[u] < 0) {  // 未访问
              // 检查 u 的邻居 (CSR 中 row_ptr[u] 存储的是出边指向的邻居)
              // 原代码逻辑：遍历 u 的边，如果 edge->e 在 front 中，则 u 被唤醒
              uint64_t start = graph->row_ptr[u];
              uint64_t end = graph->row_ptr[u + 1];
              for (uint64_t idx = start; idx < end; ++idx) {
                uint64_t v = graph->col_ind[idx];
                if (front.get_bit(v)) {
                  bfs_dist[u] = distance;
                  awake_count++;
                  curr.set_bit(u);  // 对应 next.set_bit(u)
                  break;
                }
              }
            }
          }
          front.swap(curr);
          distance++;
        } while ((awake_count >= old_awake_count) ||
                 (awake_count > (int64_t)num_vertices / beta));

// Bitmap to Queue
#pragma omp parallel
        {
          gapbs::QueueBuffer<int64_t> lqueue(queue);
#pragma omp for schedule(dynamic, 64)
          for (uint64_t n = 0; n < num_vertices; n++) {
            if (front.get_bit(n)) lqueue.push_back(n);
          }
          lqueue.flush();
        }
        queue.slide_window();
        scout_count = 1;

      } else {
        // === Top-Down Step ===
        edges_to_check -= scout_count;
        scout_count = 0;

// 严格对应 do_bfs_TDStep
#pragma omp parallel reduction(+ : scout_count)
        {
          gapbs::QueueBuffer<int64_t> lqueue(queue);
#pragma omp for schedule(dynamic, 64)
          for (auto q_iter = queue.begin(); q_iter < queue.end(); q_iter++) {
            int64_t u = *q_iter;
            uint64_t start = graph->row_ptr[u];
            uint64_t end = graph->row_ptr[u + 1];

            for (uint64_t idx = start; idx < end; ++idx) {
              uint64_t v = graph->col_ind[idx];
              int64_t curr_val = bfs_dist[v];

              if (curr_val < 0) {  // 对应 bfs_dist[dst] < 0
                // 使用 gapbs::compare_and_swap (假设 gapbs.h 中有，如果没有则用
                // built-in)
                if (gapbs::compare_and_swap(bfs_dist[v], curr_val, distance)) {
                  lqueue.push_back(v);
                  scout_count +=
                      -curr_val;  // 原代码这里加的是 -(-1) = 1 或者 -(-degree)
                  // 原代码逻辑是 distances 初始化为 -degree 或 -1。CSR 初始化为
                  // -1。 如果原代码逻辑是 += 1，这里也是 += 1 如果原代码逻辑是
                  // += degree，这里需要调整。 根据 BFS.cpp: distances[n] =
                  // out_degree != 0 ? -out_degree : -1; 但为了 CSR
                  // 简单性，我们通常认为 += 1 或 += degree(v)。 这里简化为 += 1
                  // (或者使用 graph->out_degree(v)) 为了完全对齐 scout_count
                  // 的含义（剩余边数估计），加上度数更准确： scout_count +=
                  // graph->out_degree(v);
                }
              }
            }
          }
          lqueue.flush();
        }
        queue.slide_window();
        distance++;
      }
    }
    // output_result("bfs.output", bfs_dist);
  }

  // ==========================================================
  // SSSP: Delta Stepping (严格对应 SSSP.cpp)
  // ==========================================================
  void sssp(uint64_t raw_source, double delta = 1.0) {
    if (graph->raw_to_dense.find(raw_source) == graph->raw_to_dense.end())
      return;
    uint64_t source = graph->raw_to_dense[raw_source];

    const double INF = std::numeric_limits<double>::infinity();
    sssp_dist.assign(graph->num_nodes, INF);
    sssp_dist[source] = 0.0;

    // 获取总边数初始化 frontier
    gapbs::pvector<uint64_t> frontier(graph->num_edges);
    size_t shared_indexes[2] = {0, std::numeric_limits<size_t>::max() / 2};
    size_t frontier_tails[2] = {1, 0};
    frontier[0] = source;

#pragma omp parallel
    {
      std::vector<std::vector<uint64_t>> local_bins(0);
      size_t iter = 0;

      while (shared_indexes[iter & 1] !=
             std::numeric_limits<size_t>::max() / 2) {
        size_t& curr_bin_index = shared_indexes[iter & 1];
        size_t& next_bin_index = shared_indexes[(iter + 1) & 1];
        size_t& curr_frontier_tail = frontier_tails[iter & 1];
        size_t& next_frontier_tail = frontier_tails[(iter + 1) & 1];

#pragma omp for nowait schedule(dynamic, 64)
        for (size_t i = 0; i < curr_frontier_tail; i++) {
          uint64_t u = frontier[i];

          // 对应 SSSP.cpp 的优化判断
          if (sssp_dist[u] >= delta * static_cast<double>(curr_bin_index)) {
            uint64_t start = graph->row_ptr[u];
            uint64_t end = graph->row_ptr[u + 1];

            for (uint64_t idx = start; idx < end; ++idx) {
              uint64_t v = graph->col_ind[idx];
              double w =
                  graph->weights
                      [idx];  // 对应
                              // *reinterpret_cast<double*>(&prop->properties)

              double old_dist = sssp_dist[v];
              double new_dist = sssp_dist[u] + w;

              if (new_dist < old_dist) {
                bool changed_dist = true;
                while (!gapbs::compare_and_swap(sssp_dist[v], old_dist,
                                                new_dist)) {
                  old_dist = sssp_dist[v];
                  if (old_dist <= new_dist) {
                    changed_dist = false;
                    break;
                  }
                }

                if (changed_dist) {
                  size_t dest_bin = new_dist / delta;
                  if (dest_bin >= local_bins.size()) {
                    local_bins.resize(dest_bin + 1);
                  }
                  local_bins[dest_bin].push_back(v);
                }
              }
            }
          }
        }

        // 更新全局 bin 索引 (逻辑与 SSSP.cpp 完全一致)
        for (size_t i = curr_bin_index; i < local_bins.size(); i++) {
          if (!local_bins[i].empty()) {
#pragma omp critical
            next_bin_index = std::min(next_bin_index, i);
            break;
          }
        }

#pragma omp barrier
#pragma omp single nowait
        {
          curr_bin_index = std::numeric_limits<size_t>::max() / 2;
          curr_frontier_tail = 0;
        }

        if (next_bin_index < local_bins.size()) {
          size_t copy_start = gapbs::fetch_and_add(
              next_frontier_tail, local_bins[next_bin_index].size());
          std::copy(local_bins[next_bin_index].begin(),
                    local_bins[next_bin_index].end(),
                    frontier.data() + copy_start);
          local_bins[next_bin_index].clear();
        }

        iter++;
#pragma omp barrier
      }
    }
    // output_result("output_sssp.result", sssp_dist);
  }

  // ==========================================================
  // PageRank (严格对应 PageRank.cpp)
  // 注意：PageRank.cpp 中是遍历 v 的边来累加 neighbor 的 contribution。
  // 这在 CSR 语境下意味着：对于 v，我们把 v 所有出边的目标节点的
  // outgoing_contrib 加起来。
  // ==========================================================
  void pagerank(int iterations = 20, double damping = 0.85) {
    uint64_t num_vertices = graph->num_nodes;
    pr_scores.assign(num_vertices, 1.0 / num_vertices);

    // PageRank.cpp 使用了 outgoing_contrib[4] 的 vector of pvectors 结构
    // 这里简化逻辑，只用 outgoing_contrib[0] 即可，因为原代码虽然分配了4个，
    // 但核心计算只用了 outgoing_contrib[0]
    std::vector<double> outgoing_contrib(num_vertices, 0.0);

    const double base_score = (1.0 - damping) / num_vertices;
    std::vector<uint64_t> degrees(num_vertices);

// 获取度数
#pragma omp parallel for
    for (uint64_t v = 0; v < num_vertices; v++) {
      degrees[v] = graph->out_degree(v);
    }

    for (int iter = 0; iter < iterations; ++iter) {
      double dangling_sum = 0.0;

// 1. 计算 Outgoing Contribution (对应 PageRank.cpp 第一步)
#pragma omp parallel for reduction(+ : dangling_sum)
      for (uint64_t v = 0; v < num_vertices; v++) {
        uint64_t out_degree = degrees[v];
        if (out_degree == 0) {
          dangling_sum += pr_scores[v];
        } else {
          outgoing_contrib[v] = pr_scores[v] / out_degree;
        }
      }
      dangling_sum /= num_vertices;

// 2. 更新分数 (对应 PageRank.cpp 第二步)
#pragma omp parallel for schedule(dynamic, 64)
      for (uint64_t v = 0; v < num_vertices; v++) {
        double incoming_total = 0;
        if (degrees[v]) {
          // 严格复刻原代码逻辑：
          // GraphAlgorithms::for_each_edge(ds, ... incoming_total +=
          // outgoing_contrib[edge->e] ...)
          uint64_t start = graph->row_ptr[v];
          uint64_t end = graph->row_ptr[v + 1];
          for (uint64_t idx = start; idx < end; ++idx) {
            uint64_t neighbor = graph->col_ind[idx];
            incoming_total += outgoing_contrib[neighbor];
          }
        }
        pr_scores[v] = base_score + damping * (incoming_total + dangling_sum);
      }
    }
    // output_result("output_pr.result", pr_scores);
  }

  // 辅助函数：输出结果并映射回 Raw ID
  template <typename T>
  void output_result(std::string filename, const std::vector<T>& data) {
    std::vector<std::pair<uint64_t, T>> sorted_res;
    sorted_res.reserve(graph->num_nodes);

    for (uint64_t i = 0; i < graph->num_nodes; ++i) {
      sorted_res.push_back({graph->dense_to_raw[i], data[i]});
    }

    // 按照 Raw ID 排序输出，方便 diff 对比
    std::sort(sorted_res.begin(), sorted_res.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    std::ofstream out(filename);
    for (const auto& p : sorted_res) {
      out << p.first << " " << p.second << "\n";
    }
    out.close();
  }
};

#endif  // CSR_BASELINE_H
