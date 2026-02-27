#pragma once

#include <cstddef>
#include <cstdint>

namespace lldb {

uint32_t Hash(const char *data, size_t n, uint32_t seed);

};  