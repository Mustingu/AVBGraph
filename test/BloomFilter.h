#include <bitset>
#include <functional>
#include <iostream>
#include <vector>

class IntegerBloomFilter {
 private:
  std::vector<bool> bit_array;  // 位数组
  int size;                     // 位数组大小
  int num_hashes;               // 哈希函数数量

  // 哈希函数1
  size_t hash1(int key) const {
    std::hash<int> hasher;
    return hasher(key) % size;
  }

  // 哈希函数2
  size_t hash2(int key) const {
    // 使用另一个简单的哈希算法
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = (key >> 16) ^ key;
    return key % size;
  }

 public:
  // 构造函数
  IntegerBloomFilter(int size, int num_hashes)
      : size(size), num_hashes(num_hashes) {
    bit_array.resize(size, false);
  }

  // 添加元素
  void add(int key) {
    size_t h1 = hash1(key);
    size_t h2 = hash2(key);

    for (int i = 0; i < num_hashes; ++i) {
      size_t combined_hash = (h1 + i * h2) % size;
      bit_array[combined_hash] = true;
    }
  }

  // 检查元素是否存在（可能有误判）
  bool contains(int key) const {
    size_t h1 = hash1(key);
    size_t h2 = hash2(key);

    for (int i = 0; i < num_hashes; ++i) {
      size_t combined_hash = (h1 + i * h2) % size;
      if (!bit_array[combined_hash]) {
        return false;
      }
    }
    return true;
  }

  // 清空布隆过滤器
  void clear() { std::fill(bit_array.begin(), bit_array.end(), false); }

  // 获取位数组大小
  int getSize() const { return size; }

  // 获取哈希函数数量
  int getNumHashes() const { return num_hashes; }
};

// int main() {
//   // 创建一个布隆过滤器，位数组大小为1000，使用5个哈希函数
//   IntegerBloomFilter bloomFilter(1000, 5);

//   // 添加一些整数
//   bloomFilter.add(42);
//   bloomFilter.add(1337);
//   bloomFilter.add(123456);
//   bloomFilter.add(-99);  // 支持负整数

//   // 测试存在性
//   std::cout << "Contains 42: " << bloomFilter.contains(42)
//             << std::endl;  // 应该输出1 (true)
//   std::cout << "Contains 1337: " << bloomFilter.contains(1337)
//             << std::endl;  // 应该输出1 (true)
//   std::cout << "Contains 999: " << bloomFilter.contains(999)
//             << std::endl;  // 可能输出0 (false)

//   // 测试误判率
//   int false_positives = 0;
//   int tests = 10000;
//   for (int i = 2000; i < 2000 + tests; ++i) {
//     if (bloomFilter.contains(i)) {
//       false_positives++;
//     }
//   }
//   std::cout << "False positive rate: " << (false_positives * 100.0 / tests)
//             << "%" << std::endl;

//   return 0;
// }
