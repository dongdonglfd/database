#pragma once

#include "util/macros.h"
#include "util/slice.h"
#include "util/status.h"
#include "options.h"
namespace lldb {
class Iterator {
 public:
  Iterator();

  DISALLOW_COPY_AND_MOVE(Iterator);

  virtual ~Iterator();

  //判断当前迭代器是否处于有效状态。只有在指向有效的键值对时，Valid() 才返回 true。

  //迭代器在越过数据的末尾或开始时会变为无效
  virtual bool Valid() const = 0;

  //将迭代器移动到数据的 第一个 键值对
  virtual void SeekToFirst() = 0;

  //将迭代器移动到数据的 最后一个 键值对
  virtual void SeekToLast() = 0;

  //查找并跳转到给定 key 的位置。
  //如果该键存在，迭代器将指向该键值对。如果不存在，它将指向该键 之后的第一个键值对
  virtual void Seek(const Slice &target) = 0;

  //将迭代器移动到 下一个 键值对。
  //通过 Next()，你可以按顺序遍历数据。每次调用，迭代器将返回下一个键值对。
  virtual void Next() = 0;

  //将迭代器移动到 上一个 键值对。
  //常用于反向遍历数据
  virtual void Prev() = 0;
  //获取当前迭代器指向的键值对的 键 和 值。
  // Return the key for the current entry.  The underlying storage for
  // the returned slice is valid only until the next modification of
  // the iterator.
  // REQUIRES: Valid()
  virtual Slice key() const = 0;

  // Return the value for the current entry.  The underlying storage for
  // the returned slice is valid only until the next modification of
  // the iterator.
  // REQUIRES: Valid()
  virtual Slice value() const = 0;

  // If an error has occurred, return it.  Else return an ok status.
  virtual Status status() const = 0;

  using CleanupFunction = void (*)(void *arg1, void *arg2);
  void RegisterCleanup(CleanupFunction function, void *arg1, void *arg2);

 private:
  // Cleanup functions are stored in a single-linked list.
  // The list's head node is inlined in the iterator.
  struct CleanupNode {
    // True if the node is not used. Only head nodes might be unused.
    bool IsEmpty() const { return function == nullptr; }
    // Invokes the cleanup function.
    void Run() {
      assert(function != nullptr);
      (*function)(arg1, arg2);
    }

    // The head node is used if the function pointer is not null.
    CleanupFunction function;
    void *arg1;
    void *arg2;
    CleanupNode *next;
  };
  CleanupNode cleanup_head_;
};

// Return an empty iterator with the specified status.
Iterator *NewErrorIterator(const Status &status);

// Return an empty iterator (yields nothing).
Iterator *NewEmptyIterator();

// Return a new two level iterator.  A two-level iterator contains an
// index iterator whose values point to a sequence of blocks where
// each block is itself a sequence of key,value pairs.  The returned
// two-level iterator yields the concatenation of all key/value pairs
// in the sequence of blocks.  Takes ownership of "index_iter" and
// will delete it when no longer needed.
//
// Uses a supplied function to convert an index_iter value into
// an iterator over the contents of the corresponding block.
Iterator *NewTwoLevelIterator(
    Iterator *index_iter,
    Iterator *(*block_function)(void *arg, const ReadOptions &options,
                                const Slice &index_value),
    void *arg, const ReadOptions &options);
} 