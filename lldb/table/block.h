#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "iterator.h"

namespace lldb {

struct BlockContents;
class Comparator;

class Block {
 public:
  // 接收一块已经读取好的数据
  explicit Block(const BlockContents &contents);

  Block(const Block &) = delete;
  
  Block &operator=(const Block &) = delete;

  ~Block();

  size_t size() const { return size_; }
  //创建一个 Block 内部的迭代器
  Iterator *NewIterator(const Comparator *comparator);

 private:
  class Iter;

  uint32_t NumRestarts() const;//返回 restart 点数量

  // +-------------------+---------------------+----------------------+
  // | Data              | Restart Points      | Num Restarts         |
  // +-------------------+---------------------+----------------------+
  const char *data_;
  size_t size_;
  uint32_t restart_offset_;  // Offset in data_ of restart array
  bool owned_;               // Block owns data_[]
};

struct Options;

class BlockBuilder {
 public:
  explicit BlockBuilder(const Options *options);

  DISALLOW_COPY(BlockBuilder);

  // 重置 builder
  void Reset();

  // key is larger than any previously added key
  void Add(const Slice &key, const Slice &value);

  // Finish building the block and return a slice that refers to the
  // block contents.  The returned slice will remain valid for the
  // lifetime of this builder or until Reset() is called.
  Slice Finish();

  //估算 block 当前大小
  size_t CurrentSizeEstimate() const;

  // 判断是否没有数据
  bool empty() const { return buffer_.empty(); }

 private:
  const Options *options_;          //每隔多少个 key 插入一个 restart point
  std::string buffer_;              // 存最终的 block 数据
  std::vector<uint32_t> restarts_;  // 记录 restart 点（写阶段用）
  int counter_;                     // 离上一个 restart 已经写了多少个 key
  bool finished_;                   //是否已经调用过 Finish()
  std::string last_key_;
};

} 
