//
// Created by masitan on 11/22/24.
//

#ifndef MST_SORTLEDTON_MYTIME_H
#define MST_SORTLEDTON_MYTIME_H

#include <string.h>

#include <chrono>

void PrintFunctionTime(std::function<void()> function, std::string str) {
  auto start = std::chrono::high_resolution_clock::now();
  // 执行你的代码，比如延迟1秒
  function();
  // 结束时间点
  auto end = std::chrono::high_resolution_clock::now();
  // 计算时间差
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  // 输出经过的时间，单位为ms
  std::cout << str << " Elapsed time: " << duration.count() << "ms\n\n";
}

#endif  // MST_SORTLEDTON_MYTIME_H
