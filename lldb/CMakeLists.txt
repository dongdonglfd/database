cmake_minimum_required(VERSION 3.16)

project(lldb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)

find_package(Threads REQUIRED)

find_path(SNAPPY_INCLUDE_DIR NAMES snappy.h REQUIRED)
find_library(SNAPPY_LIBRARY NAMES snappy REQUIRED)

find_path(ZSTD_INCLUDE_DIR NAMES zstd.h REQUIRED)
find_library(ZSTD_LIBRARY NAMES zstd REQUIRED)

set(LLDB_SOURCES
    db/db.cc
    db/env.cc
    db/filename.cc
    db/keyformat.cc
    db/version_edit.cc
    db/version_set.cc
    db/wal.cc
    db/write_batch.cc
    table/block.cc
    table/filter_block.cc
    table/iterator.cc
    table/lrucache.cc
    table/memtable.cc
    table/options.cc
    table/merger.cc
    table/skiplist.cc
    table/table.cc
    table/table_builder.cc
    table/table_cache.cc
    table/table_format.cc
    util/arena.cc
    util/Comparator.cc
    util/compressor.cc
    util/crc32c.cc
    util/encoding.cc
    util/filter_policy.cc
    util/hash.cc
    wisckey/file_manager.cc
    wisckey/table_builder.cc
    wisckey/vfile_builder.cc
    wisckey/vtable_format.cc)

add_library(lldb ${LLDB_SOURCES})

target_include_directories(
    lldb
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${SNAPPY_INCLUDE_DIR}
        ${ZSTD_INCLUDE_DIR})

target_link_libraries(
    lldb
    PUBLIC
        Threads::Threads
    PRIVATE
        ${SNAPPY_LIBRARY}
        ${ZSTD_LIBRARY})

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(lldb PRIVATE -Wall -Wextra)
endif()

enable_testing()

add_executable(lldb_smoke_test tests/db_smoke_test.cc)
target_link_libraries(lldb_smoke_test PRIVATE lldb)

add_test(NAME lldb_smoke_test COMMAND lldb_smoke_test)
