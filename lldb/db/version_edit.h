#pragma once

#include <set>
#include <vector>

#include "keyformat.h"
#include "util/format.h"
#include "util/status.h"

#include "wisckey/vtable_format.h"

using namespace lldb::wisckey;

namespace lldb {


// 这个结构体用于描述一个 SSTable 文件（.sst 文件）的核心元数据。
struct FileMetaData {
  FileMetaData() : refs(0), allowed_seeks(1 << 10), file_size(0) {}

  int refs;             // 引用计数。当一个 Version 不再使用某个文件时，此计数减一。当减到 0 时，该物理文件可以被安全删除。
  int allowed_seeks;    // 允许的 seek 次数。这是一个优化，当 seek 次数过多时，会触发 compaction，以优化查询性能。
  uint64_t number;      // 文件的唯一编号。
  uint64_t file_size;   // 文件大小（字节）。
  InternalKey smallest; // 该文件包含的最小的内部键 (InternalKey)。
  InternalKey largest;  // 该文件包含的最大的内部键 (InternalKey)。
};
//描述“当前数据库版本发生了哪些变化”
class VersionEdit {
 public:
  VersionEdit() { Clear(); };
  ~VersionEdit() = default;

  void Clear();
//记录数据库全局元数据
  void SetLogNumber(uint64_t num) {
    has_log_number_ = true;
    log_number_ = num;
  }
  
  void SetPrevLogNumber(uint64_t num) {
    has_prev_log_number_ = true;
    prev_log_number_ = num;
  }
  void SetNextFile(uint64_t num) {
    has_next_file_number_ = true;
    next_file_number_ = num;
  }
  void SetLastSequence(SequenceNumber seq) {
    has_last_sequence_ = true;
    last_sequence_ = seq;
  }
  // L45-L47: SetCompactPointer 方法
  // 记录下一次在某个 level 的 compaction 应该从哪个 key 开始。
  // 这可以确保 compaction 工作是公平的，不会总是从头开始合并文件。
  void SetCompactPointer(int level, const InternalKey &key) {
    compact_pointers_.push_back(std::make_pair(level, key));
  }

  // 记录一个变更：在指定的 level 新增了一个 SSTable 文件。
  // 文件的所有元数据（大小、键范围等）都被封装在 FileMetaData 对象中。
  void AddFile(int level, uint64_t file, uint64_t file_size,
               const InternalKey &smallest, const InternalKey &largest) {
    FileMetaData f;
    f.number = file;
    f.file_size = file_size;
    f.smallest = smallest;
    f.largest = largest;
    new_files_.push_back(std::make_pair(level, f));
  }
  // 记录一个变更：在指定的 level 移除了一个文件（通过文件编号）。
  // 这通常发生在 Compaction 过程中，旧的、被合并的文件会被移除。
  void RemoveFile(int level, uint64_t file) {
    deleted_files_.insert(std::make_pair(level, file));
  }
  //// 这部分是针对 Wisckey 论文实现的 KV 分离架构的扩展
  void AddVFile(std::shared_ptr<VFileMeta> file) {
    added_vfiles_.push_back(file);
  }
  
  void DeleteVFile(uint64_t file_number) {
    deleted_vfiles_.push_back(file_number);
  }
  // EncodeTo: 将 VersionEdit 对象中的所有变更信息序列化成一个字符串，以便写入 MANIFEST 文件。
  // DecodeFrom: 从一个 Slice（通常是从 MANIFEST 文件中读取的）中解析出 VersionEdit 的内容。
  void EncodeTo(std::string* dst) const;
  Status DecodeFrom(const Slice& src);

 private:
  friend class VersionSet;

  typedef std::set<std::pair<int, uint64_t>> DeletedFileSet;

  uint64_t log_number_;//当前wal文件
  uint64_t prev_log_number_;
  uint64_t next_file_number_;//下一个文件编号
  SequenceNumber last_sequence_;//当前最大序列号
  //这个字段有没有被设置（可选更新）
  bool has_log_number_;
  bool has_prev_log_number_;
  bool has_next_file_number_;
  bool has_last_sequence_;

  std::vector<std::pair<int, InternalKey>> compact_pointers_;//每一层 compaction 扫描到哪里了
  DeletedFileSet deleted_files_;
  std::vector<std::pair<int, FileMetaData>> new_files_;//在 level 层新增了哪些 SST 文件

  // Vtable
  std::vector<std::shared_ptr<VFileMeta>> added_vfiles_;
  std::vector<uint64_t> deleted_vfiles_;// 删除的 Value Log 文件
};
}  
