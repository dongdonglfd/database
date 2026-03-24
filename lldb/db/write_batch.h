#pragma once

#include <cstdint>

#include "table/memtable.h"
#include "util/slice.h"
#include "util/status.h"

namespace lldb {

class Slice;
class MenTable;

class WriteBatch {
 public:
  class Handler {
   public:
    virtual ~Handler();
    virtual void Put(const Slice &key, const Slice &value) = 0;
    virtual void Delete(const Slice &key) = 0;
  };

  WriteBatch();

  // Intentionally copyable.
  WriteBatch(const WriteBatch &) = default;

  WriteBatch &operator=(const WriteBatch &) = default;

  ~WriteBatch();

  // 向这个批处理包中添加一个“插入或更新”操作。
  //这个操作并不会立即执行，只是被序列化后追加到内部的 rep_ 字符串中。
  void Put(const Slice &key, const Slice &value);

  // 向包中添加一个“删除”操作，同样也只是追加到 rep_ 中。
  void Delete(const Slice &key);

  //  清空这个批处理包中的所有操作，实际上就是清空 rep_ 字符串。
  void Clear();

  // The size of the database changes caused by this batch.
  //
  // This number is tied to implementation details, and may change across
  // releases. It is intended for LevelDB usage metrics.
  //返回这个批处理包序列化后大约占用的内存大小
  size_t ApproximateSize() const;

  // Copies the operations in "source" to this batch.
  //
  // This runs in O(source size) time. However, the constant factor is better
  // than calling Iterate() over the source batch with a Handler that replicates
  // the operations into this batch.
  //把另一个 batch 拼进来
  void Append(const WriteBatch &source);

  // Support for iterating over the contents of a batch.
  //它会遍历内部的 rep_ 字符串，解析出每一个操作
  Status Iterate(Handler *handler) const;

 private:
  friend class WriteBatchInternal;

 /*
  * | sequence_number |    count     |                   data                            |
  * |    fixed64      |   fixed32    |  ValueType  | key_size | key | value_size | value |
  */
  std::string rep_;
};

// WriteBatchInternal provides static methods for manipulating a
// WriteBatch that we don't want in the public WriteBatch interface.
class WriteBatchInternal {
 public:
  // 读取 batch 里有多少条操作，从 rep_ 的第 8~12 字节读出 count
  static int Count(const WriteBatch *batch);

  // 修改操作数量，Put / Delete 后更新 count
  static void SetCount(WriteBatch *batch, int n);

  // 获取这个 batch 的起始序列号，数据库要保证写入顺序
  static SequenceNumber Sequence(const WriteBatch *batch);

  // 给 batch 分配 sequence number，DB::Write() 时设置
  static void SetSequence(WriteBatch *batch, SequenceNumber seq);
  //写 WAL（日志）
  static Slice Contents(const WriteBatch *batch) { return Slice(batch->rep_); }
  //rep_ 的大小（字节数）
  static size_t ByteSize(const WriteBatch *batch) { return batch->rep_.size(); }
  //直接替换 rep_
  static void SetContents(WriteBatch *batch, const Slice &contents);
  //把 batch 应用到 MemTable
  static Status InsertInto(const WriteBatch *batch, MemTable *memtable);
  //把 src 拼到 dst 后面
  static void Append(WriteBatch *dst, const WriteBatch *src);
};
}  
