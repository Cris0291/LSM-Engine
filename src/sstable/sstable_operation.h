#pragma once

#include "memtable.h"
#include <cstddef>
#include <cstdint>
#include <vector>

enum class ReadBlockResult : unsigned char { GOOD, ERROR };

struct IndexEntry {
  std::uint64_t offset;
  std::vector<std::byte> key;
};

struct RecordSstable {
  std::vector<std::byte> key;
  std::vector<std::byte> value;
  OperationRecord op;
};
