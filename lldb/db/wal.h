#pragma once

#include <cstddef>

#include "env.h"
#include "util/format.h"
#include "util/macros.h"
#include "util/slice.h"
#include "util/status.h"
#include "util/crc32c.h"

namespace lldb {

namespace crc32c = lldb::crc32c;

using namespace lldb::log;

// WritebaleFile is a specific write file class
// Wal divides log files into blocks
class Wal {

 public:
  explicit Wal(WritableFile *dest);

  Wal(WritableFile *dest, uint64_t dest_length);

  DISALLOW_COPY(Wal);

  ~Wal();

  Status AddRecord(const Slice &slice);

 private:
 // 这是一个私有的辅助函数，是实际执行物理写入的底层实现。
  Status EmitPhysicalRecord(RecordType type, const char *ptr, size_t length);
 // 指向底层可写文件对象的指针。
  WritableFile *dest_;
  int block_offset_;

  uint32_t type_crc_[kMaxRecordType + 1];
};

} 
