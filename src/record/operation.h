#include <cstddef>
#include <span>
enum class OperationRecord : unsigned char { PUT, DELETE };

enum class DecodeStatus { GOOD, TRUNCATED, CORRUPTED };

struct DecodeResult {
  DecodeStatus status;
  std::span<std::byte> record;
  std::size_t bytes_read;
};
