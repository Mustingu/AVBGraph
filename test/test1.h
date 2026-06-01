//
// Created by masitan on 11/22/24.
//

#ifndef MST_SORTLEDTON_TEST1_H
#define MST_SORTLEDTON_TEST1_H

#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "MyTime.h"
#include "tbb/concurrent_map.h"

class Version {
 public:
  Version *next;
  int timestamp;
};

#endif  // MST_SORTLEDTON_TEST1_H
