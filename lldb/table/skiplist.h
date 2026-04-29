#pragma once

#include <assert.h>
#include <atomic>
#include <new>

#include "util/arena.h"
#include "util/macros.h"
#include "util/random.h"

namespace lldb {

// Format of an Key is concatenation of:
//  key_size     : varint32 of internal_key.size()
//  key bytes    : char[internal_key.size()]
//  tag          : uint64((sequence << 8) | type)
//  value_size   : varint32 of value.size()
//  value bytes  : char[value.size()]

template <typename Key, class Comparator>
struct SkipNode { 
  explicit SkipNode(const Key &k) : key(k) {}

  const Key key;//不可变数据结构

  auto Next(int n) const -> SkipNode * {//返回第 n 层的 next 指针
    return next_[n].load(std::memory_order_acquire);
  }

  void SetNext(int n, SkipNode *x) {//把第 n 层的 next 指针设置为 x
    next_[n].store(x, std::memory_order_release);
  }

  // No-barrier variants that can be safely used in a few locations.
  SkipNode *NoBarrier_Next(int n) {
    assert(n >= 0);
    return next_[n].load(std::memory_order_relaxed);
  }
  void NoBarrier_SetNext(int n, SkipNode *x) {
    assert(n >= 0);
    next_[n].store(x, std::memory_order_relaxed);
  }

 private:
  std::atomic<SkipNode *> next_[0];//变长数组：sizeof(SkipNode) + height * sizeof(atomic<SkipNode*>)
};

template <typename Key, class Comparator>
class SkipList {
  using Node = lldb::SkipNode<Key, Comparator>;

 public:
  explicit SkipList(Comparator cmp, Arena *arena);

  DISALLOW_COPY_AND_MOVE(SkipList);

  // Insert key into the list.
  void Insert(const Key &key);

  // Find key in SkipList
  auto lookup(const Key &key) const -> bool;

  class Iterator {
   public:
    inline explicit Iterator(const SkipList *list) {
      list_ = list;
      node_ = nullptr;
    }

    // Returns true iff the iterator is positioned at a valid node.
    inline auto Valid() const -> bool { return node_ != nullptr; }//判断是否有效。

    // Returns the key at the current position.
    inline auto key() const -> const Key & { return node_->key; }//返回当前 key

    // Advances to the next position.
    inline void Next() { node_ = node_->Next(0); }//往下一节点（level 0）

    inline void Prev() {
      assert(Valid());
      node_ = list_->FindLessThan(node_->key);
      if (node_ == list_->head_) {
        node_ = nullptr;
      }
    }

    inline void Seek(const Key &target) {//跳到 >= target 的第一个节点
      node_ = list_->FindGreaterOrEqual(target, nullptr);
    }

    inline void SeekToFirst() { node_ = list_->head_->Next(0); }//跳到第一个元素

    inline void SeekToLast() {//跳到最后一个元素
      node_ = list_->FindLast();
      if (node_ == list_->head_) {
        node_ = nullptr;
      }
    }

   private:
    const SkipList *list_;
    Node *node_;
  };

 private:
  const static int MaxHeight = 12;//最大高度

  inline int GetMaxHeight() const {
    return max_height_.load(std::memory_order_relaxed);
  }

  auto FindGreaterOrEqual(const Key &key, Node **prev) const -> Node *;

  auto FindLast() const -> Node *;

  auto FindLessThan(const Key &key) const -> Node *;

  auto NewNode(const Key &key, int height) -> Node *;

  auto RandomHeight() -> int;

  Comparator const compare_;//比较器

  // Arena used for allocations of nodes
  Arena *const arena_;//内存池

  Node *const head_;//头节点

  // Height of the entire list
  std::atomic<int> max_height_;//当前最大高度

  lldb::Random rnd_;//随机数生成器

};

template <typename Key, class Comparator>
SkipList<Key, Comparator>::SkipList(Comparator cmp, Arena *arena)
    : compare_(cmp),
      arena_(arena),
      head_(NewNode(Key{}, MaxHeight)),
      max_height_(1),
      rnd_(0xdeadbeef) {
  for (int i = 0; i < MaxHeight; i++) {
    head_->SetNext(i, nullptr);
  }
}

template <typename Key, class Comparator>
auto SkipList<Key, Comparator>::RandomHeight() -> int {
  static const unsigned int kBranching = 4;
  int height = 1;
  while (height < MaxHeight && rnd_.OneIn(kBranching)) {
    height++;
  }
  assert(height > 0);
  assert(height <= MaxHeight);
  return height;
}

template <typename Key, class Comparator>
auto SkipList<Key, Comparator>::NewNode(const Key &key, int height) -> Node * {
  char *memory = arena_->AllocateAligned(
      sizeof(Node) + sizeof(std::atomic<Node *>) * height);
  return new (memory) Node(key);
}

template <typename Key, class Comparator>
auto SkipList<Key, Comparator>::lookup(const Key &key) const -> bool {
  Node *node = FindGreaterOrEqual(key, nullptr);
  if (node != nullptr && compare_(key, node->key) == 0) {
    return true;
  }
  return false;
}

template <typename Key, class Comparator>
void SkipList<Key, Comparator>::Insert(const Key &key) {
  Node *prev[MaxHeight];
  Node *cur = FindGreaterOrEqual(key, prev);

  int height = RandomHeight();
  if (height > GetMaxHeight()) {
    for (int i = GetMaxHeight(); i < height; i++) {
      prev[i] = head_;
    }
    max_height_.store(height, std::memory_order_relaxed);
  }

  cur = NewNode(key, height);
  for (int i = 0; i < height; i++) {
    cur->NoBarrier_SetNext(i, prev[i]->NoBarrier_Next(i));
    prev[i]->SetNext(i, cur);
  }
}

template <typename Key, class Comparator>
auto SkipList<Key, Comparator>::FindGreaterOrEqual(const Key &key,
                                                   Node **prev) const
    -> Node * {
  Node *x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    Node *next = x->Next(level);
    if ((next != nullptr) && (compare_(next->key, key) < 0)) {
      x = next;
    } else {
      if (prev != nullptr) {
        prev[level] = x;
      }
      if (level == 0) {
        return next;
      }
      level--;
    }
  }
}

template <typename Key, class Comparator>
auto SkipList<Key, Comparator>::FindLast() const -> Node * {
  Node *x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    Node *next = x->Next(level);
    if (next == nullptr) {
      if (level == 0) {
        return x;
      }
      level--;
    } else {
      x = next;
    }
  }
}

template <typename Key, class Comparator>
auto SkipList<Key, Comparator>::FindLessThan(const Key &key) const -> Node * {
  Node *x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    assert(x == head_ || compare_(x->key, key) < 0);
    Node *next = x->Next(level);
    if (next == nullptr || compare_(next->key, key) >= 0) {
      if (level == 0) {
        return x;
      }
      level--;
    } else {
      x = next;
    }
  }
}
}
