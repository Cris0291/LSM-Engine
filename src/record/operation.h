#include <cstddef>
#include <span>
enum class OperationRecord : unsigned char { PUT, DELETE, PUT_BLOB_REFERENCE };

enum class DecodeStatus { GOOD, TRUNCATED, CORRUPTED };

struct DecodeResult {
  DecodeStatus status;
  OperationRecord op;
  std::span<std::byte> key;
  std::span<std::byte> value;
  std::size_t bytes_read;
};
