#include "options.h"

#include "db/env.h"
#include "util/Comparator.h"

namespace lldb {

Options::Options() : comparator(BytewiseComparator()), env(Env::Default()) {}

}  // namespace lldb
