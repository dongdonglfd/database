#pragma once

#include "iterator.h"
#include "util/Comparator.h"

namespace lldb {

Iterator *NewMergingIterator(const Comparator *comparator, Iterator **children,
                             int n);

}  // namespace lldb
