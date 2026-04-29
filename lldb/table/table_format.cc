#include "table_format.h"

#include "util/encoding.h"
#include "util/status.h"


#include "db/env.h"
#include "table/options.h"
#include "util/port.h"
#include "table/block.h"
#include "util/crc32c.h"
namespace lldb {

const BlockHandle BlockHandle::kNullBlockHandle(0, 0);

// 将 BlockHandle 的内容以 Varint64 格式编码，并追加到目标字符串 dst。
// @param dst 一个指向 std::string 的指针，编码后的结果将追加到这个字符串的末尾。
void BlockHandle::EncodeTo(std::string *dst) const {
  // --- 健全性检查 ---
  // 这是一个调试模式下的断言，用于确保在编码之前，
  // offset_ 和 size_ 都已经被赋予了有效的值。
  // ~static_cast<uint64_t>(0) 是 uint64_t 的最大值，通常被用作未初始化的标记。
  assert(offset_ != ~static_cast<uint64_t>(0));
  assert(size_ != ~static_cast<uint64_t>(0));

  // --- 变长编码 ---
  // 调用 PutVarint64 将 offset_ 编码并追加到 dst。
  PutVarint64(dst, offset_);
  // 调用 PutVarint64 将 size_ 编码并追加到 dst。
  PutVarint64(dst, size_);
}
//这个函数是 EncodeTo 的逆过程。
//它负责从一个 Slice 中反序列化（解码）出一个 BlockHandle
Status BlockHandle::DecodeFrom(Slice *input) {
  if (GetVarint64(input, &offset_) && GetVarint64(input, &size_)) {
    return Status::OK();
  } else {
    return Status::Corruption("bad block handle");
  }
}
// 将 Footer 的内容编码成一个固定长度（kEncodedLength，通常为 48 字节）的字符串，
// 并追加到目标字符串 dst。
void Footer::EncodeTo(std::string* dst) const {
  const size_t original_size = dst->size();
  metaindex_handle_.EncodeTo(dst);
  index_handle_.EncodeTo(dst);
  dst->resize(2 * BlockHandle::kMaxEncodedLength);  // Padding
  PutFixed32(dst, static_cast<uint32_t>(kTableMagicNumber & 0xffffffffu));
  PutFixed32(dst, static_cast<uint32_t>(kTableMagicNumber >> 32));
  assert(dst->size() == original_size + kEncodedLength);
  (void)original_size;  // Disable unused variable warning.
}
//这个函数是 EncodeTo 的逆过程，它的核心功能是：从一个 Slice（通常是从文件末尾读取的固定 48 字节）中安全地解析（反序列化）出一个 Footer 对象。
Status Footer::DecodeFrom(Slice* input) {
  if (input->size() < kEncodedLength) {
    return Status::Corruption("not an sstable (footer too short)");
  }

  const char* magic_ptr = input->data() + kEncodedLength - 8;
  const uint32_t magic_lo = DecodeFixed32(magic_ptr);
  const uint32_t magic_hi = DecodeFixed32(magic_ptr + 4);
  const uint64_t magic = ((static_cast<uint64_t>(magic_hi) << 32) |
                          (static_cast<uint64_t>(magic_lo)));
  if (magic != kTableMagicNumber) {
    return Status::Corruption("not an sstable (bad magic number)");
  }

  Status result = metaindex_handle_.DecodeFrom(input);
  if (result.ok()) {
    result = index_handle_.DecodeFrom(input);
  }
  if (result.ok()) {
    // We skip over any leftover data (just padding for now) in "input"
    const char* end = magic_ptr + 8;
    *input = Slice(end, input->data() + input->size() - end);
  }
  return result;
}
// 从文件中读取由 handle 指定的块。
Status ReadBlock(RandomAccessFile* file, const ReadOptions& options,
                 const BlockHandle& handle, BlockContents* result) {
  result->data = Slice();
  result->cachable = false;
  result->heap_allocated = false;

  // Read the block contents as well as the type/crc footer.
  // See table_builder.cc for the code that built this structure.
  size_t n = static_cast<size_t>(handle.size());
  char* buf = new char[n + kBlockTrailerSize];
  Slice contents;
  Status s = file->Read(handle.offset(), n + kBlockTrailerSize, &contents, buf);
  if (!s.ok()) {
    delete[] buf;
    return s;
  }
  if (contents.size() != n + kBlockTrailerSize) {
    delete[] buf;
    return Status::Corruption("truncated block read");
  }
  // Check the crc of the type and the block contents
  const char* data = contents.data();  // Pointer to where Read put the data
  if (options.verify_checksums) {
    const uint32_t crc = crc32c::Unmask(DecodeFixed32(data + n + 1));
    const uint32_t actual = crc32c::Value(data, n + 1);
    if (actual != crc) {
      delete[] buf;
      s = Status::Corruption("block checksum mismatch");
      return s;
    }
  }
  switch (data[n]) {
    case kNoCompression:
      if (data != buf) {
        // File implementation gave us pointer to some other data.
        // Use it directly under the assumption that it will be live
        // while the file is open.
        delete[] buf;
        result->data = Slice(data, n);
        result->heap_allocated = false;
        result->cachable = false;  // Do not double-cache
      } else {
        result->data = Slice(buf, n);
        result->heap_allocated = true;
        result->cachable = true;
      }

      // Ok
      break;
    case kSnappyCompression:
    case kZstdCompression:
    default:
      delete[] buf;
      return Status::NotSupported("compression not supported");
  }

  return Status::OK();
}
}
