#pragma once

#include <cstdint>
#include <string>

#include "util/slice.h"
#include "util/status.h"

namespace lldb {

class Block;
class RandomAccessFile;
class ReadOptions;

// BlockHandle is a pointer to the extent of a file that stores a data
// block or a meta block.
class BlockHandle {
 public:
  // Maximum encoding length of a BlockHandle
  enum { kMaxEncodedLength = 10 + 10 };

  BlockHandle();
  BlockHandle(uint64_t offset, uint64_t size);

  // The offset of the block in the file.
  uint64_t offset() const { return offset_; }
  void set_offset(uint64_t offset) { offset_ = offset; }

  // The size of the stored block
  uint64_t size() const { return size_; }
  void set_size(uint64_t size) { size_ = size; }

  void EncodeTo(std::string *dst) const;
  Status DecodeFrom(Slice *input);

  static const BlockHandle &NullBlockHandle() { return kNullBlockHandle; }

 private:
  uint64_t offset_;//这个 Block 在文件中的起始位置（字节偏移）
  uint64_t size_;

  static const BlockHandle kNullBlockHandle;
};

// Footer encapsulates the fixed information stored at the tail
// end of every table file.
class Footer {
 public:
  // Encoded length of a Footer.  Note that the serialization of a
  // Footer will always occupy exactly this many bytes.  It consists
  // of two block handles and a magic number.
  enum { kEncodedLength = 2 * BlockHandle::kMaxEncodedLength + 8 };

  Footer() = default;

  // The block handle for the metaindex block of the table
  const BlockHandle &metaindex_handle() const { return metaindex_handle_; }
  void set_metaindex_handle(const BlockHandle &h) { metaindex_handle_ = h; }

  // The block handle for the index block of the table
  const BlockHandle &index_handle() const { return index_handle_; }
  void set_index_handle(const BlockHandle &h) { index_handle_ = h; }

  void EncodeTo(std::string *dst) const;
  Status DecodeFrom(Slice *input);

 private:
  BlockHandle metaindex_handle_;
  BlockHandle index_handle_;
};

static const uint64_t kTableMagicNumber = 0x2045346560706835ull;
//表示“已经读出来的一个 Block 的数据”
struct BlockContents {
  Slice data;           // Block 的实际数据
  bool cachable;        // 是否可以放入缓存（Block Cache）
  bool heap_allocated;  // True iff caller should delete[] data.data()
};

// 32-bit crc
// TODO : Introducing compression algorithms
static const size_t kBlockTrailerSize = 4;

// Read the block identified by "handle" from "file".  On failure
// return non-OK.  On success fill *result and return OK.
Status ReadBlock(RandomAccessFile *file, const ReadOptions &options,
                 const BlockHandle &handle, BlockContents *result);

inline BlockHandle::BlockHandle()
    : offset_(~static_cast<uint64_t>(0)), size_(~static_cast<uint64_t>(0)) {}
inline BlockHandle::BlockHandle(uint64_t _offset, uint64_t _size)
    : offset_(_offset), size_(_size) {}
}  
// 查找 key：

// ① 读取 Footer
// ② 得到 Index Block 的 BlockHandle
// ③ 读取 Index Block
// ④ 找到目标 key 对应的 BlockHandle
// ⑤ 读取 Data Block
// ⑥ 在 Block 内查找 key（Iter）