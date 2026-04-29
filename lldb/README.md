# lldb

一个基于 C++17 的 KV 存储库实现，代码结构接近 LevelDB，并包含 `wisckey/` 目录下的 value log 相关组件。

## 构建依赖

- CMake 3.16 或更高版本
- 支持 C++17 的编译器，例如 GCC / Clang
- `pthread`
- Snappy 开发包：头文件 `snappy.h` 和对应库文件
- Zstandard 开发包：头文件 `zstd.h` 和对应库文件

在 Debian / Ubuntu 上可以参考：

```bash
sudo apt install build-essential cmake libsnappy-dev libzstd-dev
```

## 构建步骤

推荐使用 out-of-source build：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

如果只想使用默认配置，也可以直接：

```bash
cmake -S . -B build
cmake --build build --parallel
```

## 构建产物

当前工程只生成一个库目标 `lldb`，默认会输出共享库：

```text
build/liblldb.so
```

这是因为根目录 `CMakeLists.txt` 中默认设置了：

```cmake
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
```

如果希望生成静态库，可以在配置阶段关闭该选项：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build build --parallel
```

## 依赖查找说明

工程通过 CMake 的 `find_path()` 和 `find_library()` 查找 Snappy / Zstd。
如果依赖没有安装在系统默认搜索路径，可以在配置时手动指定：

```bash
cmake -S . -B build \
  -DSNAPPY_INCLUDE_DIR=/path/to/include \
  -DSNAPPY_LIBRARY=/path/to/libsnappy.so \
  -DZSTD_INCLUDE_DIR=/path/to/include \
  -DZSTD_LIBRARY=/path/to/libzstd.so
```

## 其他说明

- 当前工程主要产物仍然是库 `lldb`。
- 仓库额外提供了一个最小 smoke test 可执行文件 `lldb_smoke_test`。
- CMake 已开启 `CMAKE_EXPORT_COMPILE_COMMANDS`，配置完成后会生成 `build/compile_commands.json`。

## 运行测试

当前仓库带有一个最小 smoke test，用于验证单进程下的基本读写流程：

```text
Open -> Put -> Get -> Delete
```

先构建测试目标：

```bash
cmake -S . -B build
cmake --build build --target lldb_smoke_test --parallel
```

直接运行测试可执行文件：

```bash
./build/lldb_smoke_test
```

或者通过 CTest 运行：

```bash
ctest --test-dir build --output-on-failure -R lldb_smoke_test
```

测试运行时会使用临时目录 `/tmp/lldb-smoke-db`，结束后会自动清理。

## 已验证命令

以下命令已在当前仓库中验证通过：

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure -R lldb_smoke_test
```
