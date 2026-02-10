#include "arena.h"

#include <atomic>

namespace lldb {

static const int kBlockSize = 4096;

Arena::Arena()
    : alloc_ptr_(nullptr),alloc_bytes_remaining_(0),memory_usage_(0) {}
Arena::~Arena(){
    for(size_t i=0;i<blocks_.size();i++){
        delete[] blocks_[i];
    }
}
auto Arena::AllocateFallback(size_t bytes) -> char * {
  if (bytes > kBlockSize / 4) {
    char *result = AllocateNewBlock(bytes);//大对象直接单独分配
    return result;
  }

  alloc_ptr_ = AllocateNewBlock(kBlockSize);//小对象：新开一个标准 block
  alloc_bytes_remaining_ = kBlockSize;

  char *result = alloc_ptr_;
  alloc_ptr_ += bytes;
  alloc_bytes_remaining_ -= bytes;
  return result;
}

auto Arena::AllocateAligned(size_t bytes) -> char * {//分配一块“按对齐要求对齐”的内存
  const int align = (sizeof(void *) > 8) ? sizeof(void *) : 8;
  size_t current_mod = reinterpret_cast<uintptr_t>(alloc_ptr_) & (align - 1);
  size_t slop = (current_mod == 0 ? 0 : align - current_mod);
  size_t needed = bytes + slop;

  char *result;
  if (needed <= alloc_bytes_remaining_) {
    result = alloc_ptr_ + slop;
    alloc_ptr_ += needed;
    alloc_bytes_remaining_ -= needed;
  } else {
    result = AllocateFallback(bytes);
  }
  return result;
}

auto Arena::AllocateNewBlock(size_t block_bytes) -> char * {
  char *result = new char[block_bytes];
  blocks_.push_back(result);
  memory_usage_.fetch_add(block_bytes + sizeof(char *),
                          std::memory_order_relaxed);//写是原子的，不会撕裂
  return result;
}
}  
