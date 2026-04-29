#pragma once

#include <cstdint>
#include <vector>

#include "util/macros.h"
#include "util/slice.h"

namespace lldb {

class FilterPolicy;

// A FilterBlockBuilder is used to construct all of the filters for a
// particular Table.  It generates a single string which is stored as
// a special block in the Table.
class FilterBlockBuilder {
 public:
  explicit FilterBlockBuilder(const FilterPolicy *);

  DISALLOW_COPY(FilterBlockBuilder);

  void StartBlock(uint64_t block_offset);
  void AddKey(const Slice &key);
  Slice Finish();

 private:
  void GenerateFilter();

  const FilterPolicy *policy_;
  std::string keys_;             // Flattened key contents
  std::vector<size_t> start_;    // Starting index in keys_ of each key (offset)
  std::string result_;           // Filter data computed so far
  std::vector<Slice> tmp_keys_;  // policy_->CreateFilter() argument
  std::vector<uint32_t> filter_offsets_;
};

class FilterBlockReader {
 public:
  FilterBlockReader(const FilterPolicy *policy, const Slice &contents);
  bool KeyMayMatch(uint64_t block_offset, const Slice &key);

 private:
  const FilterPolicy *policy_;
  const char *data_;    // filter 数据区的起始地址
  const char *offset_;  // 每个 data block 对应的 filter 在 data_ 里的位置
  size_t num_;          // 有多少个 filter（= data block 数量）
  size_t base_lg_;      // 基础 log2(data block 数量)
};
} 
// 写 Data Block（存 key/value）
//         ↓
// 每个 block 的 key 被喂给 FilterBlockBuilder
//         ↓
// 生成 Bloom Filter
//         ↓
// Finish() → 得到一个 Filter Block
//         ↓
// 写入 SST 文件
