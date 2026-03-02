#pragma once

#include <string>

namespace lldb {

class Slice;

class FilterPolicy {
 public:
  virtual ~FilterPolicy();

  //输入：
  // keys[0..n-1]   有序 key 列表
  //输出：
  // 把 filter 数据 append 到 dst
  virtual void CreateFilter(const Slice *keys, int n,
                            std::string *dst) const = 0;

//   给你一个 key
// 给你 filter 数据
// 判断：
//     key 是否可能存在
  virtual bool KeyMayMatch(const Slice &key, const Slice &filter) const = 0;
};

// Return a new filter policy that uses a bloom filter with approximately
// the specified number of bits per key.  A good value for bits_per_key
// is 10, which yields a filter with ~ 1% false positive rate.
//
// Callers must delete the result after any database that is using the
// result has been closed.
//
// Note: if you are using a custom comparator that ignores some parts
// of the keys being compared, you must not use NewBloomFilterPolicy()
// and must provide your own FilterPolicy that also ignores the
// corresponding parts of the keys.  For example, if the comparator
// ignores trailing spaces, it would be incorrect to use a
// FilterPolicy (like NewBloomFilterPolicy) that does not ignore
// trailing spaces in keys.
const FilterPolicy *NewBloomFilterPolicy(int bits_per_key);

}  // namespace Tskydb
