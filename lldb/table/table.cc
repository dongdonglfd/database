#include "table.h"

#include "filter_block.h"
#include "table_format.h"

namespace lldb {

struct Table::Rep {
  Rep(const Options &options, RandomAccessFile *file)
      : options(options), file(file), index_block(nullptr), filter(nullptr) {}

  Options options;
  RandomAccessFile *file;
  Block *index_block;
  FilterBlockReader *filter;
};

Status Table::Open(const Options &options, RandomAccessFile *file,
                   uint64_t /*file_size*/, Table **table) {
  *table = nullptr;
  (void)options;
  (void)file;
  return Status::NotSupported("table open is not implemented");
}

Table::~Table() {
  delete rep_->filter;
  delete rep_->index_block;
  delete rep_;
}

Iterator *Table::NewIterator(const ReadOptions & /*options*/) const {
  return NewErrorIterator(Status::NotSupported(
      "table iterator is not implemented"));
}

uint64_t Table::ApproximateOffsetOf(const Slice & /*key*/) const { return 0; }

Iterator *Table::BlockReader(void * /*arg*/, const ReadOptions & /*options*/,
                             const Slice & /*index_value*/) {
  return NewErrorIterator(Status::NotSupported(
      "table block reader is not implemented"));
}

Status Table::InternalGet(const ReadOptions & /*options*/, const Slice & /*k*/,
                          void * /*arg*/,
                          void (* /*handle_result*/)(void *, const Slice &,
                                                     const Slice &)) {
  return Status::NotSupported("table get is not implemented");
}

void Table::ReadMeta(const Footer & /*footer*/) {}

void Table::ReadFilter(const Slice & /*filter_handle_value*/) {}

}  // namespace lldb
