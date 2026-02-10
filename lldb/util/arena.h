#pragma once

#include <cstdint>
#include <vector>
#include <atomic>

#include "util/macros.h"

namespace lldb {
class Arena {
 public:
  Arena();

  ~Arena();

  DISALLOW_COPY_AND_MOVE(Arena);

  auto Allocate(size_t bytes) -> char *;

  auto AllocateAligned(size_t bytes) -> char *;

  size_t MemoryUsage() const {
    return memory_usage_.load(std::memory_order_relaxed);
  }

 private:
  char *AllocateFallback(size_t bytes);
  char *AllocateNewBlock(size_t block_bytes);

  char *alloc_ptr_;//当前分配位置
  size_t alloc_bytes_remaining_;//当前 block 还剩多少字节

  std::vector<char *> blocks_;//每次向系统申请一大块内存（如 4KB / 8KB）

  std::atomic<size_t> memory_usage_;//统计总共向系统申请了多少内存
};

inline auto Arena::Allocate(size_t bytes) -> char * {
  if (bytes <= alloc_bytes_remaining_) {
    char *result = alloc_ptr_;
    alloc_ptr_ += bytes;
    alloc_bytes_remaining_ -= bytes;
    return result;
  }
  return AllocateFallback(bytes);
}

}  