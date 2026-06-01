#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "test1.h"

const unsigned long long CPU_FREQUENCY = 1.4e9;

extern __inline unsigned long long
    __attribute__((__gnu_inline__, __always_inline__, __artificial__))
    __rdtsc(void) {
  return __builtin_ia32_rdtsc();
}
extern __inline unsigned long long
    __attribute__((__gnu_inline__, __always_inline__, __artificial__))
    __rdtscp(unsigned int *__A) {
  return __builtin_ia32_rdtscp(__A);
}

__inline__ uint64_t perf_counter(void) {
  __asm__ __volatile__("" : : : "memory");
  uint64_t r = __rdtsc();
  __asm__ __volatile__("" : : : "memory");

  return r;
}

const int gettimes = 100000000;
void test_perf_counter() {
  for (int i = 0; i < gettimes; i++) {
    perf_counter();
  }
}

void test_chrono_now() {
  for (int i = 0; i < gettimes; i++) {
    auto start = std::chrono::high_resolution_clock::now();
  }
}

void test_clock_gettime() {
  struct timespec ts;
  for (int i = 0; i < gettimes; i++) {
    clock_gettime(CLOCK_MONOTONIC, &ts);
  }
}

void testperf_counter() {
  uint64_t start = perf_counter();

  std::this_thread::sleep_for(std::chrono::milliseconds(10000));
  uint64_t end = perf_counter();
  std::cout << start << " " << end << " "
            << (double)(end - start) * 1000 / CPU_FREQUENCY << std::endl;
}