#include <boost/asio.hpp>
#include <iostream>
#include <vector>

void thread_func(int param) {
  // 线程函数体
}

int main() {
  boost::asio::thread_pool pool(4);
  int n = 4;
  auto func = std::bind(thread_func, 10);
  boost::asio::post(pool, []() { std::cout << "here\n"; });
  pool.join();  // 等待第一个任务完成

  boost::asio::post(pool, []() { std::cout << "next\n"; });
  pool.join();  // 等待第二个任务完成
  return 0;
}