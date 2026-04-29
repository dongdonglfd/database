#include "merger.h"

#include <cassert>

namespace lldb {
namespace {

class IteratorWrapper {
 public:
  IteratorWrapper() : iter_(nullptr) {}

  void Set(Iterator *iter) { iter_ = iter; }

  Iterator *iter() const { return iter_; }
  bool Valid() const { return iter_ != nullptr && iter_->Valid(); }
  Slice key() const {
    assert(Valid());
    return iter_->key();
  }
  Slice value() const {
    assert(Valid());
    return iter_->value();
  }
  Status status() const {
    assert(iter_ != nullptr);
    return iter_->status();
  }
  void Next() {
    assert(iter_ != nullptr);
    iter_->Next();
  }
  void Prev() {
    assert(iter_ != nullptr);
    iter_->Prev();
  }
  void Seek(const Slice &target) {
    assert(iter_ != nullptr);
    iter_->Seek(target);
  }
  void SeekToFirst() {
    assert(iter_ != nullptr);
    iter_->SeekToFirst();
  }
  void SeekToLast() {
    assert(iter_ != nullptr);
    iter_->SeekToLast();
  }

 private:
  Iterator *iter_;
};

class MergingIterator : public Iterator {
 public:
  MergingIterator(const Comparator *comparator, Iterator **children, int n)
      : comparator_(comparator),
        children_(new IteratorWrapper[n]),
        n_(n),
        current_(nullptr),
        direction_(kForward) {
    for (int i = 0; i < n_; ++i) {
      children_[i].Set(children[i]);
    }
  }

  ~MergingIterator() override {
    for (int i = 0; i < n_; ++i) {
      delete children_[i].iter();
    }
    delete[] children_;
  }

  bool Valid() const override { return current_ != nullptr; }

  void SeekToFirst() override {
    for (int i = 0; i < n_; ++i) {
      children_[i].SeekToFirst();
    }
    FindSmallest();
    direction_ = kForward;
  }

  void SeekToLast() override {
    for (int i = 0; i < n_; ++i) {
      children_[i].SeekToLast();
    }
    FindLargest();
    direction_ = kReverse;
  }

  void Seek(const Slice &target) override {
    for (int i = 0; i < n_; ++i) {
      children_[i].Seek(target);
    }
    FindSmallest();
    direction_ = kForward;
  }

  void Next() override {
    assert(Valid());

    if (direction_ != kForward) {
      for (int i = 0; i < n_; ++i) {
        IteratorWrapper *child = &children_[i];
        if (child != current_) {
          child->Seek(key());
          if (child->Valid() && comparator_->Compare(key(), child->key()) == 0) {
            child->Next();
          }
        }
      }
      direction_ = kForward;
    }

    current_->Next();
    FindSmallest();
  }

  void Prev() override {
    assert(Valid());

    if (direction_ != kReverse) {
      for (int i = 0; i < n_; ++i) {
        IteratorWrapper *child = &children_[i];
        if (child != current_) {
          child->Seek(key());
          if (child->Valid()) {
            while (comparator_->Compare(key(), child->key()) == 0) {
              child->Prev();
              if (!child->Valid()) {
                break;
              }
            }
          } else {
            child->SeekToLast();
          }
        }
      }
      direction_ = kReverse;
    }

    current_->Prev();
    FindLargest();
  }

  Slice key() const override {
    assert(Valid());
    return current_->key();
  }

  Slice value() const override {
    assert(Valid());
    return current_->value();
  }

  Status status() const override {
    for (int i = 0; i < n_; ++i) {
      if (!children_[i].status().ok()) {
        return children_[i].status();
      }
    }
    return Status::OK();
  }

 private:
  enum Direction { kForward, kReverse };

  void FindSmallest() {
    IteratorWrapper *smallest = nullptr;
    for (int i = 0; i < n_; ++i) {
      IteratorWrapper *child = &children_[i];
      if (child->Valid()) {
        if (smallest == nullptr ||
            comparator_->Compare(child->key(), smallest->key()) < 0) {
          smallest = child;
        }
      }
    }
    current_ = smallest;
  }

  void FindLargest() {
    IteratorWrapper *largest = nullptr;
    for (int i = 0; i < n_; ++i) {
      IteratorWrapper *child = &children_[i];
      if (child->Valid()) {
        if (largest == nullptr ||
            comparator_->Compare(child->key(), largest->key()) > 0) {
          largest = child;
        }
      }
    }
    current_ = largest;
  }

  const Comparator *comparator_;
  IteratorWrapper *children_;
  int n_;
  IteratorWrapper *current_;
  Direction direction_;
};

}  // namespace

Iterator *NewMergingIterator(const Comparator *comparator, Iterator **children,
                             int n) {
  assert(n >= 0);
  return new MergingIterator(comparator, children, n);
}

}  // namespace lldb
