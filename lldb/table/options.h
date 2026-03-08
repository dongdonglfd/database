// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_INCLUDE_OPTIONS_H_
#define STORAGE_LEVELDB_INCLUDE_OPTIONS_H_

#include <cstddef>

#include "export.h"

namespace lldb {

class Cache;
class Comparator;
class Env;
class FilterPolicy;
class Logger;
class Snapshot;

// DB contents are stored in a set of blocks, each of which holds a
// sequence of key,value pairs.  Each block may be compressed before
// being stored in a file.  The following enum describes which
// compression method (if any) is used to compress a block.
enum CompressionType {
  // 注意：不要修改已有枚举值，因为这些值会写入磁盘文件，
  // 一旦写入就不可变更，否则旧数据将无法解析。
  kNoCompression = 0x0,      // 0：不压缩
  kSnappyCompression = 0x1,  // 1：使用 Snappy 压缩（默认）
  kZstdCompression = 0x2,    // 2：使用 Zstandard 压缩
};

// 用于控制数据库行为的选项（在 DB::Open 时传入）
struct LEVELDB_EXPORT Options {
  // 创建一个包含所有字段默认值的 Options 对象。
  Options();

  // -------------------
  // 影响行为的参数

  // 用于定义表中键顺序的比较器。
  // 默认值：一个使用字典序字节比较的比较器。
  //
  // 要求：客户端必须确保此处提供的比较器与先前在同一数据库上
  // 调用 open 时提供的比较器具有相同的名称，并且以完全相同的方式对键进行排序。
  const Comparator* comparator;

  // 如果为 true，则在数据库不存在时创建它。
  // 默认值：false
  bool create_if_missing = false;

  // 如果为 true，则在数据库已存在时引发错误。
  // 默认值：false
  bool error_if_exists = false;

  // 如果为 true，实现将对其正在处理的数据进行积极的检查，
  // 如果检测到任何错误，将提前停止。
  // 这可能会产生不可预见的后果：例如，一个数据库条目的损坏
  // 可能导致大量条目变得不可读，或者整个数据库无法打开。
  // 默认值：false
  bool paranoid_checks = false;

  // 使用指定对象与环境交互，例如读/写文件、调度后台工作等。
  // 默认值：Env::Default()
  Env* env;

  // 数据库生成的任何内部进度/错误信息都将写入 info_log（如果非空），
  // 或者写入与数据库内容存储在同一目录中的文件（如果 info_log 为空）。
  // 默认值：nullptr
  Logger* info_log = nullptr;

  // -------------------
  // 影响性能的参数

  // 在转换为排序的磁盘文件之前，在内存中累积的数据量（由磁盘上的未排序日志支持）。
  //
  // 较大的值可以提高性能，尤其是在批量加载期间。
  // 最多可以同时在内存中保留两个写缓冲区，因此您可能希望调整此参数以控制内存使用。
  // 此外，较大的写缓冲区将导致下次打开数据库时恢复时间更长。
  // 默认值：4MB
  size_t write_buffer_size = 4 * 1024 * 1024;

  // 数据库可以使用的打开文件数。如果您的数据库有较大的工作集，
  // 您可能需要增加此值（预算每 2MB 工作集一个打开文件）。
  // 默认值：1000
  int max_open_files = 1000;

  // 对块的控制（用户数据存储在一组块中，块是从磁盘读取的单位）。

  // 如果非空，则使用指定的缓存来缓存块。
  // 如果为空，leveldb 将自动创建并使用一个 8MB 的内部缓存。
  // 默认值：nullptr
  Cache* block_cache = nullptr;

  // 每个块中打包的用户数据的大致大小。请注意，此处指定的块大小对应于未压缩的数据。
  // 如果启用了压缩，从磁盘读取的单元的实际大小可能会更小。此参数可以动态更改。
  // 默认值：4KB
  size_t block_size = 4 * 1024;

  // 用于键的增量编码的重启点之间的键数。
  // 此参数可以动态更改。大多数客户端应保持此参数不变。
  // 默认值：16
  int block_restart_interval = 16;

  // Leveldb 在切换到新文件之前将向文件写入的最大字节数。
  // 大多数客户端应保持此参数不变。但是，如果您的文件系统对较大的文件更有效，
  // 您可以考虑增加该值。缺点是会进行更长的压缩，从而导致更长的延迟/性能抖动。
  // 增加此参数的另一个原因可能是当您最初填充大型数据库时。
  // 默认值：2MB
  size_t max_file_size = 2 * 1024 * 1024;

  // 使用指定的压缩算法压缩块。此参数可以动态更改。
  //
  // 默认值：kSnappyCompression，提供轻量级但快速的压缩。
  //
  // 在 Intel(R) Core(TM)2 2.4GHz 上 kSnappyCompression 的典型速度：
  //    ~200-500MB/s 压缩
  //    ~400-800MB/s 解压缩
  // 请注意，这些速度明显快于大多数持久性存储速度，因此通常不值得切换到 kNoCompression。
  // 即使输入数据不可压缩，kSnappyCompression 实现也会有效地检测到这一点并切换到未压缩模式。
  CompressionType compression = kSnappyCompression;

  // zstd 的压缩级别。
  // 当前仅支持 [-5,22] 范围。默认值为 1。
  int zstd_compression_level = 1;

  // 实验性功能：如果为 true，则在打开数据库时附加到现有的 MANIFEST 和日志文件。
  // 这可以显着加快打开速度。
  //
  // 默认值：当前为 false，但将来可能会变为 true。
  bool reuse_logs = false;

  // 如果非空，则使用指定的过滤器策略来减少磁盘读取。
  // 许多应用程序将受益于在此处传递 NewBloomFilterPolicy() 的结果。
  const FilterPolicy* filter_policy = nullptr;
};

// Options that control read operations
struct LEVELDB_EXPORT ReadOptions {
  // If true, all data read from underlying storage will be
  // verified against corresponding checksums.
  bool verify_checksums = false;

  // Should the data read for this iteration be cached in memory?
  // Callers may wish to set this field to false for bulk scans.
  bool fill_cache = true;

  // If "snapshot" is non-null, read as of the supplied snapshot
  // (which must belong to the DB that is being read and which must
  // not have been released).  If "snapshot" is null, use an implicit
  // snapshot of the state at the beginning of this read operation.
  const Snapshot* snapshot = nullptr;
};

// Options that control write operations
struct LEVELDB_EXPORT WriteOptions {
  WriteOptions() = default;

  // If true, the write will be flushed from the operating system
  // buffer cache (by calling WritableFile::Sync()) before the write
  // is considered complete.  If this flag is true, writes will be
  // slower.
  //
  // If this flag is false, and the machine crashes, some recent
  // writes may be lost.  Note that if it is just the process that
  // crashes (i.e., the machine does not reboot), no writes will be
  // lost even if sync==false.
  //
  // In other words, a DB write with sync==false has similar
  // crash semantics as the "write()" system call.  A DB write
  // with sync==true has similar crash semantics to a "write()"
  // system call followed by "fsync()".
  bool sync = false;
};

}  

#endif  // STORAGE_LEVELDB_INCLUDE_OPTIONS_H_