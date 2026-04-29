#pragma once

#include <cstdint>
#include <string>

#include "iterator.h"
#include "lrucache.h"
#include "options.h"
#include "table.h"
#include "util/macros.h"

namespace lldb {

class TableCache {
 public:
  TableCache(const std::string &dbname, const Options &options, int enties);

  DISALLOW_COPY(TableCache);

  ~TableCache();

  // Return an iterator for the specified file number (the corresponding
  // file length must be exactly "file_size" bytes).  If "tableptr" is
  // non-null, also sets "*tableptr" to point to the Table object
  // underlying the returned iterator, or to nullptr if no Table object
  // underlies the returned iterator.  The returned "*tableptr" object is owned
  // by the cache and should not be deleted, and is valid for as long as the
  // returned iterator is live.
  //为某个 SST 文件创建一个迭代器
  Iterator *NewIterator(const ReadOptions &options, uint64_t file_number,
                        uint64_t file_size, Table **tableptr = nullptr);

  // Evict any entry for the specified file number
  //从缓存中删除某个 SST
  void Evict(uint64_t file_number);

  // If a seek to internal key "k" in specified file finds an entry,
  // call (*handle_result)(arg, found_key, found_value).
  //在某个 SST 文件里查 key
  Status Get(const ReadOptions &options, uint64_t file_number,
             uint64_t file_size, const Slice &k, void *arg,
             void (*handle_result)(void *, const Slice &, const Slice &));

 private:
  // Find a table with a specified file number
  //找到一个 SST 对应的 Table（从 cache 或磁盘）
  Status FindTable(uint64_t file_number, uint64_t file_size,
                   LRUCache::Handle **);

  Env *const env_;//文件系统接口
  const std::string dbname_;//数据库路径
  const Options &options_;//配置
  LRUCache *cache_;//缓存
};
}  
