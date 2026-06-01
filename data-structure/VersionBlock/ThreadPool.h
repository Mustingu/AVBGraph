#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

class ThreadPool {
 public:
  ThreadPool(int numThreads) : numThreads_(numThreads) {
    for (int i = 0; i < numThreads_; ++i) {
      threads_.emplace_back([this] {
        while (true) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(queueMutex_);
            // Wait for a task to be available
            condition_.wait(lock, [this] { return !tasks_.empty(); });
            task = std::move(tasks_.front());
            tasks_.pop();
          }
          // Execute the task
          task();
        }
      });
    }
  }

  ~ThreadPool() {
    {
      std::unique_lock<std::mutex> lock(queueMutex_);
      stop_ = true;
    }
    condition_.notify_all();
    for (std::thread& thread : threads_) {
      thread.join();
    }
  }

  template <typename F>
  void enqueue(F&& task) {
    {
      std::unique_lock<std::mutex> lock(queueMutex_);
      tasks_.push(std::forward<F>(task));
    }
    condition_.notify_one();
  }

 private:
  int numThreads_;
  std::vector<std::thread> threads_;
  std::queue<std::function<void()>> tasks_;
  std::mutex queueMutex_;
  std::condition_variable condition_;
  bool stop_ = false;
};

#endif