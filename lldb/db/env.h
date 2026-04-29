#pragma once

#include <memory>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "util/macros.h"
#include "util/slice.h"
#include "util/status.h"
#include "util/sync.h"

namespace lldb {

class SequentialFile;
class RandomAccessFile;
class WritableFile;

using size_t = std::size_t;

constexpr const size_t kWritableFileBufferSize = 65536;//缓冲区的默认大小为 64 KB
constexpr const int kOpenBaseFlags = 0;//定义一个用于打开文件时的基础标志位

class Env {
  using BackgroundWorkFunc = std::function<void(void *)>;

 public:
  Env();
  ~Env() = default;
  static Env *Default();
  //创建 顺序读取文件
  Status NewSequentialFile(const std::string &filename,
                           SequentialFile **result);
  //创建 随机读取文件
  Status NewRandomAccessFile(const std::string &filename,
                             RandomAccessFile **result);
  //创建 可写文件
  Status NewWritableFile(const std::string &filename, WritableFile **result);
  //创建 追加文件
  Status NewAppendableFile(const std::string &filename, WritableFile **result);
  //把任务放入后台线程执行
  void Schedule(BackgroundWorkFunc background_work_function,
                void *backgroud_work_arg);
  //获取目录下的所有文件
  Status GetChildren(const std::string &directory_path,
                     std::vector<std::string> *result);

  // 删除文件
  Status RemoveFile(const std::string &filename);
  // Get File size 
  Status GetFileSize(const std::string &filename, uint64_t *size);

  // 将文件 src 重命名为 target.
  Status RenameFile(const std::string &from, const std::string &to);

  // Get current time
  uint64_t NowMicros();

 private:
  //后台线程函数
  void BackgroundThreadMain();

  //表示一个 后台任务
  struct BgWorkPackage {
    explicit BgWorkPackage(BackgroundWorkFunc func, void *arg)
        : func(std::move(func)), arg(arg) {}

    BackgroundWorkFunc func;//任务函数
    void *arg;//任务参数
  };

  std::mutex bg_work_mutex_;
  CondVar bg_work_cv_;
  bool started_background_thread_;

  std::queue<BgWorkPackage> bg_work_queue_;//保存所有任务
};

class SequentialFile {
 public:
  SequentialFile(std::string filename, int fd);

  DISALLOW_COPY(SequentialFile);

  ~SequentialFile();

  // Read up to "n" bytes from the file.  "scratch[0..n-1]" may be
  // written by this routine.  Sets "*result" to the data that was
  // read (including if fewer than "n" bytes were successfully read).
  // May set "*result" to point at data in "scratch[0..n-1]", so
  // "scratch[0..n-1]" must be live when "*result" is used.
  // If an error was encountered, returns a non-OK status.
  //
  // REQUIRES: External synchronization
  Status Read(size_t n, Slice *result, char *scratch);

  // Skip "n" bytes from the file. This is guaranteed to be no
  // slower that reading the same data, but may be faster.
  //
  // If end of file is reached, skipping will stop at the end of the
  // file, and Skip will return OK.
  //
  // REQUIRES: External synchronization
  Status Skip(uint64_t n);


 private:
  const int fd_;
  const std::string filename_;
};

// A file abstraction for randomly reading the contents of a file.
class RandomAccessFile {
 public:
  RandomAccessFile() = default;

  RandomAccessFile(const RandomAccessFile &) = delete;
  RandomAccessFile &operator=(const RandomAccessFile &) = delete;

  virtual ~RandomAccessFile() = default;

  // Read up to "n" bytes from the file starting at "offset".
  // "scratch[0..n-1]" may be written by this routine.  Sets "*result"
  // to the data that was read (including if fewer than "n" bytes were
  // successfully read).  May set "*result" to point at data in
  // "scratch[0..n-1]", so "scratch[0..n-1]" must be live when
  // "*result" is used.  If an error was encountered, returns a non-OK
  // status.
  //
  // Safe for concurrent use by multiple threads.
  virtual Status Read(uint64_t offset, size_t n, Slice *result,
                      char *scratch) const = 0;
};

class WriteableFileWriter {
 public:
  WriteableFileWriter(std::unique_ptr<WritableFile> &&file);

  ~WriteableFileWriter() = default;
  DISALLOW_COPY(WriteableFileWriter);

  // Write small data into the buffer to reduce IO operations
  // Write big data directly to file
  Status Append(const Slice &data);

  // Flash the disk
  // Close fd
  Status Close();

  // Flash the disk
  Status Flush();

  // Ensure that memory and disk data are consistent
  Status Sync();

  uint64_t GetFileSize() { return filesize_; }

 private:
  uint64_t filesize_;
  std::unique_ptr<WritableFile> writable_file_;
};

class WritableFile {
 public:
  WritableFile(std::string filename, int fd);

  ~WritableFile();

  DISALLOW_COPY(WritableFile);

  // Write small data into the buffer to reduce IO operations
  // Write big data directly to file
  Status Append(const Slice &data);

  // Flash the disk
  // Close fd
  Status Close();

  // Flash the disk
  Status Flush();

  // Ensure that memory and disk data are consistent
  Status Sync();

 private:
  Status FlushBuffer();//把buf_写入fd

  Status WriteUnbuffered(const char *data, size_t size);//直接write，不经过buffer

  Status SyncDirIfManifest();//MANIFEST 文件

  static Status SyncFd(int fd, const std::string &fd_path);//封装：fsync(fd)

  // Returns the directory name in a path pointing to a file.
  //
  // Returns "." if the path does not contain any directory separator.
  //返回目录
  static std::string Dirname(const std::string &filename) {
    std::string::size_type separator_pos = filename.rfind('/');
    if (separator_pos == std::string::npos) {
      return std::string(".");
    }
    // The filename component should not contain a path separator. If it does,
    // the splitting was done incorrectly.
    assert(filename.find('/', separator_pos + 1) == std::string::npos);

    return filename.substr(0, separator_pos);
  }

  // Extracts the file name from a path pointing to a file.
  //
  // The returned Slice points to |filename|'s data buffer, so it is only valid
  // while |filename| is alive and unchanged.
  static Slice Basename(const std::string &filename) {
    std::string::size_type separator_pos = filename.rfind('/');
    if (separator_pos == std::string::npos) {
      return Slice(filename);
    }
    // The filename component should not contain a path separator. If it does,
    // the splitting was done incorrectly.
    assert(filename.find('/', separator_pos + 1) == std::string::npos);

    return Slice(filename.data() + separator_pos + 1,
                 filename.length() - separator_pos - 1);
  }

  // True if the given file is a manifest file.
  static bool IsManifest(const std::string &filename) {
    return Basename(filename).starts_with("MANIFEST");
  }

  // buf_[0, pos_ - 1] contains data to be written to fd_.
  char buf_[kWritableFileBufferSize];
  size_t pos_;
  int fd_;

  const bool is_manifest_;  // True if the file's name starts with MANIFEST.
  const std::string filename_;
  const std::string dirname_;  // The directory of filename_.
};

} 
