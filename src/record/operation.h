#include <cstddef>
#include <span>
#include <vector>
enum class OperationRecord : unsigned char { PUT, DELETE, PUT_BLOB_REFERENCE };

enum class DecodeStatus { GOOD, TRUNCATED, CORRUPTED };

struct DecodeResult {
  DecodeStatus status;
  OperationRecord op;
  std::span<std::byte> key;
  std::span<std::byte> value;
  std::size_t bytes_read;
};

struct Record {
  OperationRecord op;
  std::vector<std::byte> key;
  std::vector<std::byte> value;
  Record(const DecodeResult &decode_result) : op(decode_result.op) {
    key.assign(decode_result.key.cbegin(), decode_result.key.cend());
    value.assign(decode_result.value.cbegin(), decode_result.value.cend());
  }
};
