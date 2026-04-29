#include "iterator.h"

#include <assert.h>



namespace lldb {

class IteratorWrapper {
 public:
  IteratorWrapper() : iter_(nullptr), valid_(false) {}
  explicit IteratorWrapper(Iterator *iter) : iter_(nullptr) { Set(iter); }
  ~IteratorWrapper() { delete iter_; }
  Iterator *iter() const { return iter_; }

  void Set(Iterator *iter) {
    delete iter_;
    iter_ = iter;
    if (iter_ == nullptr) {
      valid_ = false;
    } else {
      Update();
    }
  }

  bool Valid() const { return valid_; }
  Slice key() const {
    assert(Valid());
    return key_;
  }
  Slice value() const {
    assert(Valid());
    return iter_->value();
  }
  Status status() const {
    assert(iter_);
    return iter_->status();
  }
  void Next() {
    assert(iter_);
    iter_->Next();
    Update();
  }
  void Prev() {
    assert(iter_);
    iter_->Prev();
    Update();
  }
  void Seek(const Slice &k) {
    assert(iter_);
    iter_->Seek(k);
    Update();
  }
  void SeekToFirst() {
    assert(iter_);
    iter_->SeekToFirst();
    Update();
  }
  void SeekToLast() {
    assert(iter_);
    iter_->SeekToLast();
    Update();
  }

 private:
  void Update() {
    valid_ = iter_->Valid();
    if (valid_) {
      key_ = iter_->key();
    }
  }

  Iterator *iter_;
  bool valid_;
  Slice key_;
};

Iterator::Iterator() {
  // 初始化清理链表的头节点。
  // cleanup_head_ 是一个哨兵节点，它本身不执行任何清理操作（function=nullptr），
  // 并且初始时指向空的下一个节点（next=nullptr），表示清理链表为空。
  cleanup_head_.function = nullptr;
  cleanup_head_.next = nullptr;
}

// 迭代器析构函数
Iterator::~Iterator() {
  // 检查清理链表是否不为空。
  // IsEmpty() 检查头节点（哨兵节点）是否有关联的清理函数。
  // 如果不为空，说明至少注册了一个清理任务。
  if (!cleanup_head_.IsEmpty()) {
    // 首先，执行头节点自身注册的清理函数。
    // 这是一种优化：第一个注册的清理任务会直接复用头节点，避免一次额外的内存分配。
    cleanup_head_.Run();
    // 然后，遍历并执行链表中所有后续节点的清理函数。
    for (CleanupNode* node = cleanup_head_.next; node != nullptr;) {
      // 执行当前节点的清理函数。
      node->Run();
      // 保存指向下一个节点的指针，因为当前节点即将被删除。
      CleanupNode* next_node = node->next;
      // 删除当前清理节点，释放其占用的内存。
      delete node;
      // 移动到下一个节点。
      node = next_node;
    }
  }
}

// 注册一个清理任务，该任务将在迭代器销毁时被执行。
void Iterator::RegisterCleanup(CleanupFunction func, void* arg1, void* arg2) {
  // 断言确保注册的清理函数是有效的。
  assert(func != nullptr);
  CleanupNode* node;
  // 检查清理链表是否为空（即 cleanup_head_ 哨兵节点是否被占用）。
  if (cleanup_head_.IsEmpty()) {
    // 优化：如果链表为空，直接复用哨兵节点来存储第一个清理任务。
    // 这样可以避免为最常见的情况（至少有一个清理任务）进行一次动态内存分配。
    node = &cleanup_head_;
  } else {
    // 如果哨兵节点已被占用，则为新的清理任务动态分配一个新节点。
    node = new CleanupNode();
    // 将新节点插入到链表的头部（紧跟在哨兵节点之后）。
    // 这种头插法实现简单，但会导致清理任务的执行顺序与注册顺序相反（除了第一个任务）。
    node->next = cleanup_head_.next;
    cleanup_head_.next = node;
  }
  // 将清理函数及其参数存入目标节点。
  node->function = func;
  node->arg1 = arg1;
  node->arg2 = arg2;
}

namespace {

using BlockFunction = Iterator *(*)(void *, const ReadOptions &, const Slice &);

class EmptyIterator : public Iterator {
 public:
  EmptyIterator(const Status &s) : status_(s) {}
  ~EmptyIterator() override = default;

  bool Valid() const override { return false; }
  void Seek(const Slice & /*target*/) override {}
  void SeekToFirst() override {}
  void SeekToLast() override {}
  void Next() override { assert(false); }
  void Prev() override { assert(false); }
  Slice key() const override {
    assert(false);
    return Slice();
  }
  Slice value() const override {
    assert(false);
    return Slice();
  }
  Status status() const override { return status_; }

 private:
  Status status_;
};

class TwoLevelIterator : public Iterator {
 public:
  TwoLevelIterator(Iterator *index_iter, BlockFunction block_function,
                   void *arg, const ReadOptions &options)
      : block_function_(block_function),// 设置用于生成二级数据迭代器的函数
        arg_(arg),// 设置传递给 block_function 的参数
        options_(options),// 存储读取选项
        index_iter_(index_iter),// 设置一级索引迭代器
        data_iter_(nullptr) {}// 将二级数据迭代器初始化为空，它将在需要时被懒加载

  ~TwoLevelIterator() = default;

  void Seek(const Slice &target) override {
    // 1. 在一级索引迭代器上执行 Seek 操作。
    //    这会找到第一个可能包含 target 的数据块的索引条目。
    index_iter_.Seek(target);
    // 2. 根据索引迭代器的当前位置，初始化二级数据迭代器。
    //    这会加载对应的数据块，并为其创建一个新的迭代器 (dat-iter_)。
    InitDataBlock();
    // 3. 如果数据迭代器有效，则在二级数据迭代器上执行 Seek 操作。
    //    这会在数据块内部精确定位到 >= target 的键。
    if (data_iter_.iter() != nullptr) data_iter_.Seek(target);
    // 4. 向前跳过可能为空的数据块。
    //    如果当前数据块的所有键都小于 target（导致 dat-iter_ 变为无效），
    //    或者当前数据块本身就是空的，此函数会继续移动到下一个数据块，
    //    直到找到一个有效的键，确保迭代器最终停在正确的位置。
    SkipEmptyDataBlocksForward();
  }
  // 定位到整个数据结构中的第一个键值对。
  void SeekToFirst() override {
    // 1. 将一级索引迭代器移动到其第一个条目。
    index_iter_.SeekToFirst();
    // 2. 根据索引的第一个条目，加载对应的数据块并初始化二级数据迭代器。
    InitDataBlock();
    if (data_iter_.iter() != nullptr) data_iter_.SeekToFirst();
    // 3. 如果二级数据迭代器有效，则将其移动到其内部的第一个键值对。
    SkipEmptyDataBlocksForward();
  }
  void SeekToLast() override {
    // 1. 将一级索引迭代器移动到其最后一个条目。
    index_iter_.SeekToLast();
    InitDataBlock();
    if (data_iter_.iter() != nullptr) data_iter_.SeekToLast();
    SkipEmptyDataBlocksBackward();
  }
  // 移动到下一个键值对。
  void Next() override {
    assert(Valid());
    data_iter_.Next();
    SkipEmptyDataBlocksForward();
  }
  // 移动到上一个键值对。
  void Prev() override {
    assert(Valid());
    data_iter_.Prev();
    SkipEmptyDataBlocksBackward();
  }

  bool Valid() const override { return data_iter_.Valid(); }
  Slice key() const override {
    assert(Valid());
    return data_iter_.key();
  }
  Slice value() const override {
    assert(Valid());
    return data_iter_.value();
  }
  Status status() const override {
    // It'd be nice if status() returned a const Status& instead of a Status
    if (!index_iter_.status().ok()) {
      return index_iter_.status();
    } else if (data_iter_.iter() != nullptr && !data_iter_.status().ok()) {
      return data_iter_.status();
    } else {
      return status_;
    }
  }

 private:
  void SaveError(const Status &s) {
    if (status_.ok() && !s.ok()) status_ = s;
  }
  // 向前跳过空的数据块。
  void SkipEmptyDataBlocksForward() {
    while (data_iter_.iter() == nullptr || !data_iter_.Valid()) {
      // Move to next block
      if (!index_iter_.Valid()) {
        SetDataIterator(nullptr);
        return;
      }
      index_iter_.Next();
      InitDataBlock();
      if (data_iter_.iter() != nullptr) data_iter_.SeekToFirst();
    }
  }
  //当迭代器在向后移动时（比如调用 Prev() 或 SeekToLast()），
  //如果当前位置变得无效（比如走到了一个数据块的开头，或者遇到了一个完全为空的数据块），
  //这个函数负责找到上一个真正包含数据的块，并将迭代器稳稳地停在那个新数据块的最后一个元素上。
  void SkipEmptyDataBlocksBackward() {
    while (data_iter_.iter() == nullptr || !data_iter_.Valid()) {
      // Move to next block
      if (!index_iter_.Valid()) {
        SetDataIterator(nullptr);
        return;
      }
      index_iter_.Prev();
      InitDataBlock();
      if (data_iter_.iter() != nullptr) data_iter_.SeekToLast();
    }
  }

  void SetDataIterator(Iterator *data_iter) {
    if (data_iter_.iter() != nullptr) SaveError(data_iter_.status());
    data_iter_.Set(data_iter);
  }

  void InitDataBlock() {
    if (!index_iter_.Valid()) {
      SetDataIterator(nullptr);
    } else {
      Slice handle = index_iter_.value();
      if (data_iter_.iter() != nullptr &&
          handle.compare(data_block_handle_) == 0) {
        // data_iter_ is already constructed with this iterator, so
        // no need to change anything
      } else {
        Iterator *iter = (*block_function_)(arg_, options_, handle);
        data_block_handle_.assign(handle.data(), handle.size());
        SetDataIterator(iter);
      }
    }
  }

  BlockFunction block_function_;
  void *arg_;
  const ReadOptions options_;
  Status status_;
  IteratorWrapper index_iter_;
  IteratorWrapper data_iter_;  // May be nullptr
  // If data_iter_ is non-null, then "data_block_handle_" holds the
  // "index_value" passed to block_function_ to create the data_iter_.
  std::string data_block_handle_;
};

}  // namespace

Iterator *NewErrorIterator(const Status &status) {
  return new EmptyIterator(status);
}

Iterator *NewEmptyIterator() { return new EmptyIterator(Status::OK()); }

Iterator *NewTwoLevelIterator(Iterator *index_iter,
                              BlockFunction block_function, void *arg,
                              const ReadOptions &options) {
  return new TwoLevelIterator(index_iter, block_function, arg, options);
}

}
