#include <cstddef>
#include <cstdint>
#include <vector>
enum class ReadBlockResult { GOOD, ERROR };

struct IndexEntry {
  std::uint64_t offset;
  std::vector<std::byte> key;
};
