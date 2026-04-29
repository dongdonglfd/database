#pragma once

#include <memory>
#include <mutex>
#include <set>
#include <string>

#include "table/iterator.h"
#include "table/options.h"
#include "util/macros.h"
#include "version_edit.h"

namespace lldb {

class Wal;
class TableCache;
class Compaction;
class InternalKeyComparator;
class VersionSet;
class WritableFile;

// Returns true iff some file in "files" overlaps the user key range
// [*smallest,*largest].
// smallest==nullptr represents a key smaller than all keys in the DB.
// largest==nullptr represents a key largest than all keys in the DB.
// REQUIRES: If disjoint_sorted_files, files[] contains disjoint ranges
//           in sorted order.
//判断：某一层的文件集合中，是否有文件的 key 范围与给定区间发生重叠
bool SomeFileOverlapsRange(const InternalKeyComparator &icmp,
                           bool disjoint_sorted_files,
                           const std::vector<FileMetaData *> &files,
                           const Slice *smallest_user_key,
                           const Slice *largest_user_key);

class Version {
 public:
 //哪一个文件被“频繁访问”
  struct GetStats {
    FileMetaData *seek_file;
    int seek_file_level;
  };

  // Lookup the value for key.  If found, store it in *val and
  // return OK.  Else return a non-OK status.  Fills *stats.
  //在 当前 Version 的所有 SST 文件中查找 key
  Status Get(const ReadOptions &, const LookupKey &key, std::string *val,
             GetStats *stats);

  // Adds "stats" into the current state.  Returns true if a new
  // compaction may need to be triggered, false otherwise.
  //判断是否需要触发 compaction
  bool UpdateStats(const GetStats &stats);

  // Return the level at which we should place a new memtable compaction
  // result that covers the range [smallest_user_key,largest_user_key].
  //这个新 SST 文件应该放在哪一层（L0 / L1 / L2...）
  int PickLevelForMemTableOutput(const Slice &smallest_user_key,
                                 const Slice &largest_user_key);

  // Returns true iff some file in the specified level overlaps
  // some part of [*smallest_user_key,*largest_user_key].
  // smallest_user_key==nullptr represents a key smaller than all the DB's keys.
  // largest_user_key==nullptr represents a key largest than all the DB's keys.
  bool OverlapInLevel(int level, const Slice *smallest_user_key,
                      const Slice *largest_user_key);
  //找出：[level] 中所有与 [begin, end] 重叠的文件
  void GetOverlappingInputs(
      int level,
      const InternalKey *begin,  // nullptr means before all keys
      const InternalKey *end,    // nullptr means after all keys
      std::vector<FileMetaData *> *inputs);
  //管理 Version 的引用计数
  void Ref();
  void Unref();

 private:
  friend class VersionSet;
  friend class Compaction;

  class LevelFileNumIterator;

  // Call func(arg, level, f) for every file that overlaps user_key in
  // order from newest to oldest.  If an invocation of func returns
  // false, makes no more calls.
  //
  // REQUIRES: user portion of internal_key == user_key.
  //所有“可能包含这个 key”的 SST 文件
  void ForEachOverlapping(Slice user_key, Slice internal_key, void *arg,
                          bool (*func)(void *, int, FileMetaData *));

  explicit Version(VersionSet *vset)
      : vset_(vset),
        next_(this),
        prev_(this),
        refs_(0),
        compaction_score_(-1),
        compaction_level_(-1),
        file_to_compact_(nullptr),
        file_to_compact_level_(-1) {}

  VersionSet *vset_;  // VersionSet to which this Version belongs
  Version *next_;     // Next version in linked list
  Version *prev_;     // Previous version in linked list
  int refs_;          // Number of live refs to this version

  // List of files per level
  std::vector<FileMetaData *> files_[config::kNumLevels];

  // Level that should be compacted next and its compaction score.
  // Score < 1 means compaction is not strictly needed.  These fields
  // are initialized by Finalize().
  double compaction_score_;
  int compaction_level_;

  // Next file to compact based on seek stats.
  FileMetaData *file_to_compact_;
  int file_to_compact_level_;
};

class VersionSet {
 public:
  VersionSet(const std::string &dbname, const Options *options,
             TableCache *table_cache, const InternalKeyComparator *cmp);

  DISALLOW_COPY(VersionSet);

  ~VersionSet();

  // Apply *edit to the current version to form a new descriptor that
  // is both saved to persistent state and installed as the new
  // current version.  Will release *mu while actually writing to the file.
  // REQUIRES: *mu is held on entry.
  // REQUIRES: no other thread concurrently calls LogAndApply()
  //生成一个新 Version，并把它持久化到 MANIFEST
  Status LogAndApply(VersionEdit *edit, std::mutex *mu);

  // Recover the last saved descriptor from persistent stor先age.
  // 从 MANIFEST 文件恢复所有 Version
  Status Recover(bool *save_manifest);

  // Pick level and inputs for a new compaction.
  // Returns nullptr if there is no compaction to be done.
  //是否需要 compaction？压哪一层？哪些文件？
  Compaction *PickCompaction();

  // For GC
  // Add all files listed in any live version to *live.
  //所有 Version 正在使用的文件
  void AddLiveFiles(std::set<uint64_t> *live);

  // Return the current version.
  //获取当前 Version（查询用）
  Version *current() const { return current_; }

  // Return the current log file number.
  uint64_t LogNumber() const { return log_number_; }

  // ?
  uint64_t PrevLogNumber() const { return prev_log_number_; }

  // Return the current manifest file number
  //当前 MANIFEST 文件编号
  uint64_t ManifestFileNumber() const { return manifest_file_number_; }

  // Allocate and return a new file number
  uint64_t NewFileNumber() { return next_file_number_++; }

  // Return the last sequence number.
  //当前数据库最大 sequence number
  uint64_t LastSequence() const { return last_sequence_; }

  void SetLastSequence(uint64_t seq) { last_sequence_ = seq; }

  TableCache *GetTableCahe() const { return table_cache_; }

  // Returns true iff some level needs a compaction.
  bool NeedsCompaction() const {
    Version *v = current_;
    return (v->compaction_score_ >= 1) || (v->file_to_compact_ != nullptr);
  }
  //如果文件没用，回收编号
  void ReuseFileNumber(uint64_t file_number) {
    if (next_file_number_ == file_number + 1) {
      next_file_number_ = file_number;
    }
  }

  // Create an iterator that reads over the compaction inputs for "*c".
  // The caller should delete the iterator when no longer needed.
  //把多个 SST 合并成一个“有序流”
  Iterator *MakeInputIterator(Compaction *c);

 private:
  class Builder;

  friend class Version;
  friend class Compaction;

  void AppendVersion(Version *v);

  // Choose best_level
  // Calculate compaction_score_
  //下一次压哪一层
  void Finalize(Version *v);

  // Save current contents to *log
  //把当前 Version 全量写入 MANIFEST
  Status WriteSnapshot(Wal *log);

  // Stores the minimal range that covers all entries in inputs in
  // smallest, largest.
  //一组文件的 key 范围
  void GetRange(const std::vector<FileMetaData *> &inputs,
                InternalKey *smallest, InternalKey *largest);

  // Stores the minimal range that covers all entries in inputs1 and inputs2
  // in *smallest, *largest.
  void GetRange2(const std::vector<FileMetaData *> &inputs1,
                 const std::vector<FileMetaData *> &inputs2,
                 InternalKey *smallest, InternalKey *largest);

  // optimization : Add more files to input[0]
  //
  // input[0] : level
  // input[1] : level + 1
  //
  // Try to add more input[0]
  // without changing the input[1] layer
  void SetupOtherInputs(Compaction *c);

  Env *const env_;
  const std::string dbname_;
  const Options *const options_;
  TableCache *const table_cache_;
  const InternalKeyComparator icmp_;
  uint64_t next_file_number_;
  uint64_t manifest_file_number_;
  uint64_t last_sequence_;
  uint64_t log_number_;
  uint64_t prev_log_number_;  // 0 or backing store for memtable being compacted

  WritableFile *descriptor_file_;
  Wal *descriptor_log_;
  Version dummy_versions_;  // Head of circular doubly-linked list of versions.
  Version *current_;

  // Per-level key at which the next compaction at that level should start.
  // Either an empty string, or a valid InternalKey.
  std::string compact_pointer_[config::kNumLevels];
};

// A Compaction encapsulates information about a compaction.
class Compaction {
 public:
  ~Compaction();

  // Return the level that is being compacted.  Inputs from "level"
  // and "level+1" will be merged to produce a set of "level+1" files.
  int level() const { return level_; }

  // Return the object that holds the edits to the descriptor done
  // by this compaction.
  VersionEdit *edit() { return &edit_; }

  // Return the ith input file at "level()+which" ("which" must be 0 or 1).
  FileMetaData *input(int which, int i) const { return inputs_[which][i]; }

  int num_input_files(int which) const { return inputs_[which].size(); }

  // Maximum size of files to build during this compaction.
  uint64_t MaxOutputFileSize() const { return max_output_file_size_; }

  // Is this a trivial compaction that can be implemented by just
  // moving a single input file to the next level (no merging or splitting)
  bool IsTrivialMove() const;

  // 把所有 inputs_[0] 和 inputs_[1] 标记为删除
  void AddInputDeletions(VersionEdit *edit);

  // Detect the number of bytes overlapped with the grandparent layer
  // When it is greater than MaxGrandParentOverlapBytes, it returns true
  // means that a new output file needs to be replaced.
  //要不要切一个新的 SST 文件？
  bool ShouldStopBefore(const Slice &internal_key);

  // Returns true if the information we have available guarantees that
  // the compaction is producing data in "level+1" for which no data exists
  // in levels greater than "level+1".
  //这个 key 在 L+2 及以上是否存在？
  bool IsBaseLevelForKey(const Slice &user_key);

 private:
  friend class Version;
  friend class VersionSet;

 Compaction(const Options *options, int level);

  int level_;  // current compaction level
  uint64_t max_output_file_size_;
  Version *input_version_;
  VersionEdit edit_;

  // For ShouldStopBefore
  // State used to check for number of overlapping grandparent files
  // (parent == level_ + 1, grandparent == level_ + 2)
  std::vector<FileMetaData *> grandparents_;
  size_t grandparent_index_;  // Index in grandparent_starts_
  bool seen_key_;             // Some output key has been seen
  int64_t overlapped_bytes_;  // Bytes of overlap between current output
                              // and grandparent files

  // Each compaction reads inputs from "level_" and "level_+1"
  std::vector<FileMetaData *> inputs_[2];  // The two sets of inputs

  // State for implementing IsBaseLevelForKey
  // level_ptrs_ holds indices into input_version_->levels_: our state
  // is that we are positioned at one of the file ranges for each
  // higher level than the ones involved in this compaction (i.e. for
  // all L >= level_ + 2).
  size_t level_ptrs_[config::kNumLevels];
};
}  
