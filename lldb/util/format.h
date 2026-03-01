#pragma once
#include<stdint.h>
namespace lldb {

namespace config {
// 定义了数据库中最大的层级数
static const int kNumLevels = 7;

// 当 Level 0 中有 4 个文件时，触发 Level 0 的压缩操作
static const int kL0_CompactionTrigger = 4;

// 当 Level 0 中的文件数量达到 8 时，写操作将开始变慢。这是一个软限制，目的是避免 Level 0 文件过多导致压缩操作变得非常耗时
static const int kL0_SlowdownWritesTrigger = 8;

// 当 Level 0 中的文件数量达到 8 时，写操作将开始变慢。这是一个软限制，目的是避免 Level 0 文件过多导致压缩操作变得非常耗时
static const int kL0_StopWritesTrigger = 12;

//如果 MemTable 被压缩时没有与其他层重叠，它会被推送到的最大层级。
//默认情况下，MemTable 压缩通常发生在 Level 0 到 Level 1，但此配置允许将 MemTable 直接压缩到 Level 2，以减少一些昂贵的压缩操作
static const int kMaxMemCompactLevel = 2;

// 在数据库读取过程中，迭代数据时的字节采样间隔，通常用于性能度量或统计
static const int kReadBytesPeriod = 1048576;

}  // namespace config

// The type of operation this time
enum ValueType {
  kTypeDeletion = 0x0, // 删除操作
  kTypeValue = 0x1,// 普通值操作（即插入）
  kTypeVtableIndex = 0x2// 特殊的 vtable 索引类型（通常是 DB 特定操作）
};
//这是一个常量，表示在查找操作时默认使用的 ValueType
static const ValueType kValueTypeForSeek = kTypeValue;

typedef uint64_t  SequenceNumber;//用于标识每个写入操作的版本号（或顺序号）。
// We leave eight bits empty at the bottom so a type and sequence#
// can be packed together into 64-bits.
static const SequenceNumber kMaxSequenceNumber = ((0x1ull << 56) - 1);

namespace log {

enum RecordType {
  // Zero is reserved for preallocated files
  kZeroType = 0,//预留类型，用于预分配日志文件的空间。

  kFullType = 1,//预留类型，用于预分配日志文件的空间。

  // For fragments
  kFirstType = 2,//分片日志记录的第一个片段
  kMiddleType = 3,//分片日志记录的中间片段
  kLastType = 4//分片日志记录的最后一个片段
};
//最大的日志记录类型，通常用于限定日志记录的最大类型
static const int kMaxRecordType = kLastType;
//义日志文件的块大小，通常用来指定每个块的大小，32KB 是一个常见的块大小。
static const int kBlockSize = 32768;

static const int kHeaderSize = 4 + 2 + 1;//4字节校验和，2字节长度，1字节类型

}  // namespace log

}  
