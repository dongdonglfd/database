#pragma once

#include <string>

#include "slice.h"

namespace lldb {

class Slice;

// A Comparator object provides a total order across slices that are
// used as keys in an sstable or a database.  A Comparator implementation
// must be thread-safe since leveldb may invoke its methods concurrently
// from multiple threads.
//key 的排序方式
class Comparator {
 public:
  virtual ~Comparator();

  // Three-way comparison.  Returns value:
  //   < 0 iff "a" < "b",
  //   == 0 iff "a" == "b",
  //   > 0 iff "a" > "b"
  virtual int Compare(const Slice &a, const Slice &b) const = 0;
  //修改的是：用于 索引块（index block）里的分隔 key
  //在保证 start 小于 limit 的条件下，尽可能将 start 变得更短，并且仍然保证 start >= original_start，且 start < limit
  virtual void FindShortestSeparator(std::string *start,
                                     const Slice &limit) const = 0;

  //用于最后一个block，只需要new>old
  virtual void FindShortSuccessor(std::string *key) const = 0;
};

// Return a builtin comparator that uses lexicographic byte-wise
// ordering.  The result remains the property of this module and
// must not be deleted.
//字节级字典序
const Comparator *BytewiseComparator();

}  
