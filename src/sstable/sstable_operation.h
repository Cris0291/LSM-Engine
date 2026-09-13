#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

enum class ReadBlockResult : unsigned char { GOOD, ERROR };

struct IndexEntry {
  std::uint64_t offset;
  std::vector<std::byte> key;
};
