#ifndef STORAGE_LEVELDB_UTIL_RANDOM_H_
#define STORAGE_LEVELDB_UTIL_RANDOM_H_

#include <cstdint>

namespace lldb {


class Random {
 private:
  uint32_t seed_;

 public:
  explicit Random(uint32_t s) : seed_(s & 0x7fffffffu) {//用一个种子初始化随机数生成器
    // Avoid bad seeds.
    if (seed_ == 0 || seed_ == 2147483647L) {
      seed_ = 1;
    }
  }
  uint32_t Next() {//生成下一个伪随机数，并推进内部状态
    static const uint32_t M = 2147483647L;  // 2^31-1
    static const uint64_t A = 16807;        // bits 14, 8, 7, 5, 2, 1, 0

    uint64_t product = seed_ * A;

   
    seed_ = static_cast<uint32_t>((product >> 31) + (product & M));
   
    if (seed_ > M) {
      seed_ -= M;
    }
    return seed_;
  }
  
  uint32_t Uniform(int n) { return Next() % n; }//返回 [0, n-1] 的均匀随机数

 
  bool OneIn(int n) { return (Next() % n) == 0; }//大约 1/n 的概率返回 true

  
  uint32_t Skewed(int max_log) { return Uniform(1 << Uniform(max_log + 1)); }//生成一个“偏向小值”的随机数，返回范围[0, 2^max_log - 1]
};

}  

#endif  
