#include <string>

#include "iterator.h"
#include "memtable.h"
#include "util/encoding.h"
#include "util/macros.h"
#include "util/slice.h"

namespace lldb {

static const char *EncodeKey(std::string *scratch, const Slice &target) {
  // 1. 清空缓冲区
  //    确保 scratch 字符串是空的，以便开始一次新的编码。
  scratch->clear();
  // 2. 编码长度
  //    PutVarint32 是一个工具函数，它会将 target.size() (一个 uint32_t 类型的长度)
  //    以可变长度整数（Varint32）的格式追加到 scratch 字符串的末尾。
  //    Varint 是一种空间效率很高的整数编码方式。
  PutVarint32(scratch, target.size());
  
  scratch->append(target.data(), target.size());
  // 返回指针
  //    返回一个指向 scratch 字符串内部数据缓冲区头部的指针。
  //    这个指针现在指向的是一个 [长度][数据] 格式的内存块。
  return scratch->data();
}

static Slice GetLengthPrefixedSlice(const char *data) {
  uint32_t len;
  const char *p = data;
  p = GetVarint32Ptr(p, p + 5, &len);
  return Slice(p, len);
}

MemTable::MemTable(const InternalKeyComparator &comparator)
    : comparator_(comparator), refs_(0), table_(comparator_, &arena_) {}

MemTable::~MemTable() { assert(refs_ == 0); }

size_t MemTable::ApproximateMemoryUsage() { return arena_.MemoryUsage(); }

int MemTable::KeyComparator::operator()(const char *aptr,
                                        const char *bptr) const {
  Slice a = GetLengthPrefixedSlice(aptr);
  Slice b = GetLengthPrefixedSlice(bptr);
  return comparator.Compare(a, b);
}

void MemTable::Add(SequenceNumber s, ValueType type, const Slice &key,
                   const Slice &value) {
  // Format of an entry is concatenation of:
  //  key_size     : varint32 of internal_key.size()
  //  key bytes    : char[internal_key.size()]
  //  tag          : uint64((sequence << 8) | type)
  //  value_size   : varint32 of value.size()
  //  value bytes  : char[value.size()]
  size_t key_size = key.size();
  size_t val_size = value.size();
  size_t internal_key_size = key_size + 8;
  const size_t encoded_len = VarintLength(internal_key_size) +
                             internal_key_size + VarintLength(val_size) +
                             val_size;
  char *buf = arena_.Allocate(encoded_len);
  char *p = EncodeVarint32(buf, internal_key_size);
  std::memcpy(p, key.data(), key.size());
  p += key_size;
  EncodeFixed64(p, (s << 8) | type);
  p += 8;
  p = EncodeVarint32(p, val_size);
  std::memcpy(p, value.data(), value.size());
  assert(p + val_size == buf + encoded_len);
  table_.Insert(buf);
}

bool MemTable::Get(const LookupKey &key, std::string *value, Status *s) {
  Slice memkey = key.memtable_key();
  Table::Iterator iter(&table_);
  iter.Seek(memkey.data());

  if (iter.Valid()) {
    const char *entry = iter.key();
    uint32_t key_length;
    const char *key_ptr = GetVarint32Ptr(entry, entry + 5, &key_length);

    if (comparator_.comparator.user_comparator()->Compare(
            Slice(key_ptr, key_length - 8), key.user_key()) == 0) {
      const uint64_t tag = DecodeFixed64(key_ptr + key_length - 8);
      switch (static_cast<ValueType>(tag & 0xff)) {
        case kTypeValue:
        case kTypeVtableIndex: {
          Slice v = GetLengthPrefixedSlice(key_ptr + key_length);
          value->assign(v.data(), v.size());
          return true;
        }
        case kTypeDeletion:
          *s = Status::NotFound(Slice());
          return true;
      }
    }
  }
  return false;
}

class MemTableIterator : public Iterator {
 public:
  explicit MemTableIterator(MemTable::Table *table) : iter_(table) {}

  DISALLOW_COPY(MemTableIterator);

  ~MemTableIterator() override = default;

  bool Valid() const override { return iter_.Valid(); }
  void Seek(const Slice &k) override { iter_.Seek(EncodeKey(&tmp_, k)); }
  void SeekToFirst() override { iter_.SeekToFirst(); }
  void SeekToLast() override { iter_.SeekToLast(); }
  void Next() override { iter_.Next(); }
  void Prev() override { iter_.Prev(); }
  Slice key() const override { return GetLengthPrefixedSlice(iter_.key()); }
  Slice value() const override {
    Slice key_slice = GetLengthPrefixedSlice(iter_.key());
    return GetLengthPrefixedSlice(key_slice.data() + key_slice.size());
  }

  Status status() const override { return Status::OK(); }

 private:
  MemTable::Table::Iterator iter_;
  std::string tmp_;  // For passing to EncodeKey
};

Iterator *MemTable::NewIterator() { return new MemTableIterator(&table_); }

}  
