#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "tbb/concurrent_set.h"
#include "tbb/concurrent_unordered_map.h"
#include "tbb/concurrent_unordered_set.h"
#include "tbb/concurrent_vector.h"
#include "tbb/mutex.h"

const int thread_num = 40;
const int times = 10000000;

tbb::concurrent_set<int> set1;
tbb::concurrent_unordered_set<int> set2;
tbb::concurrent_vector<int> vec1;
std::vector<int> set3;
tbb::mutex mutex;
std::mutex mutex2;
void test_concurrent_set() {
  auto n = times / thread_num;
  for (int i = 0; i < n; i++) {
    set1.insert(i);
  }
}

void test_concurrent_unordered_set() {
  auto n = times / thread_num;
  for (int i = 0; i < n; i++) {
    set2.insert(i);
  }
}

void test_common_set() {
  auto n = times / thread_num;
  for (int i = 0; i < n; i++) {
    std::lock_guard<std::mutex> lock(mutex2);
    // tbb::mutex::scoped_lock lock_guard(mutex);
    set3.push_back(i);
  }
}

void test_concurrent_vector() {
  auto n = times / thread_num;
  for (int i = 0; i < n; i++) {
    // std::lock_guard<tbb::mutex> lock(mutex);
    // tbb::mutex::scoped_lock lock_guard(mutex);
    vec1.push_back(i);
  }
}

void thread_test_concurrent_set() {
  std::vector<std::thread> threads;
  for (int i = 0; i < thread_num; i++) {
    threads.push_back(std::thread(test_concurrent_set));
  }
  for (auto &t : threads) {
    t.join();
  }
}

void thread_test_concurrent_unordered_set() {
  std::vector<std::thread> threads;
  for (int i = 0; i < thread_num; i++) {
    threads.push_back(std::thread(test_concurrent_unordered_set));
  }
  for (auto &t : threads) {
    t.join();
  }
}

void thread_test_common_set() {
  std::vector<std::thread> threads;
  for (int i = 0; i < thread_num; i++) {
    threads.push_back(std::thread(test_common_set));
  }
  for (auto &t : threads) {
    t.join();
  }
}

void thread_test_concurrent_vector() {
  std::vector<std::thread> threads;
  for (int i = 0; i < thread_num; i++) {
    threads.push_back(std::thread(test_concurrent_vector));
  }
  for (auto &t : threads) {
    t.join();
  }
}

tbb::concurrent_unordered_map<int, int> map1;

void test_concurrent_unordered_map() {
  auto n = times / thread_num;
  for (int i = 0; i < n; i++) {
    // std::lock_guard<tbb::mutex> lock(mutex);
    // tbb::mutex::scoped_lock lock_guard(mutex);
    map1.emplace(1, 1);
  }
}

void thread_test_concurrent_unordered_map() {
  std::vector<std::thread> threads;
  for (int i = 0; i < thread_num; i++) {
    threads.push_back(std::thread(test_concurrent_unordered_map));
  }
  for (auto &t : threads) {
    t.join();
  }
}

std::unordered_map<int, int> map2;

void test_common_unordered_map() {
  auto n = times / thread_num;
  for (int i = 0; i < n; i++) {
    std::lock_guard<tbb::mutex> lock(mutex);
    // tbb::mutex::scoped_lock lock_guard(mutex);
    map2.emplace(1, 1);
  }
}

void thread_test_common_unordered_map() {
  std::vector<std::thread> threads;
  for (int i = 0; i < thread_num; i++) {
    threads.push_back(std::thread(test_common_unordered_map));
  }
  for (auto &t : threads) {
    t.join();
  }
}