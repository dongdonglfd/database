#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>

#include "db/db.h"

namespace fs = std::filesystem;

int main() {
  const fs::path db_path = fs::temp_directory_path() / "lldb-smoke-db";

  std::error_code ec;
  fs::remove_all(db_path, ec);
  ec.clear();
  fs::create_directories(db_path, ec);
  if (ec) {
    std::cerr << "failed to create db dir: " << ec.message() << '\n';
    return 1;
  }

  lldb::Options options;
  options.create_if_missing = true;
  options.write_buffer_size = 1 << 20;

  lldb::DB *raw_db = nullptr;
  lldb::Status status = lldb::DB::Open(options, db_path.string(), &raw_db);
  if (!status.ok()) {
    std::cerr << "open failed: " << status.ToString() << '\n';
    return 1;
  }

  std::unique_ptr<lldb::DB> db(raw_db);
  lldb::WriteOptions write_options;
  lldb::ReadOptions read_options;

  status = db->Put(write_options, "key", "value");
  if (!status.ok()) {
    std::cerr << "put failed: " << status.ToString() << '\n';
    return 1;
  }

  std::string value;
  status = db->Get(read_options, "key", &value);
  if (!status.ok()) {
    std::cerr << "get after put failed: " << status.ToString() << '\n';
    return 1;
  }
  if (value != "value") {
    std::cerr << "unexpected value: " << value << '\n';
    return 1;
  }

  status = db->Delete(write_options, "key");
  if (!status.ok()) {
    std::cerr << "delete failed: " << status.ToString() << '\n';
    return 1;
  }

  value.clear();
  status = db->Get(read_options, "key", &value);
  if (!status.IsNotFound()) {
    std::cerr << "expected not found after delete, got: "
              << status.ToString() << '\n';
    return 1;
  }

  fs::remove_all(db_path, ec);
  return 0;
}
