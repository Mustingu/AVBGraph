#ifndef CONCURRENT_EDGE_READER_H
#define CONCURRENT_EDGE_READER_H

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <thread>

#include "utils.h"

// 定义数据块结构
struct EdgeChunk {
  unsigned num_;
  std::vector<string> edges;
  bool is_last = false;

  EdgeChunk(unsigned num, size_t reserve_size) {
    num_ = num;
    edges.reserve(reserve_size);
  }
};

class ConcurrentEdgeReader {
 private:
  std::ifstream file;
  std::string filename;
  size_t chunk_size;

  // 双缓冲队列
  std::queue<std::shared_ptr<EdgeChunk>> buffer_queue;
  std::mutex queue_mutex;
  std::condition_variable data_available;
  std::condition_variable space_available;

  // 控制标志
  std::atomic<bool> reading_finished{false};
  std::atomic<bool> processing_finished{false};

  const size_t MAX_BUFFER_SIZE = 4;  // 最大队列长度

 public:
  ConcurrentEdgeReader(const std::string& fname, size_t c_size = (1 << 15));

  // 读取线程函数

  void reader_thread();

  // 获取下一个数据块
  std::shared_ptr<EdgeChunk> get_next_chunk();
};
void UpdateDataConcurrent(
    std::string input_file,
    std::vector<std::tuple<unsigned, unsigned, uint64_t>>& edges);
#endif  // USE_CONCURRENT_EDGE_READER