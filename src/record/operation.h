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

  bool operator==(const Record &rhs) const = default;

  Record(const DecodeResult &decode_result) : op(decode_result.op) {
    key.assign(decode_result.key.begin(), decode_result.key.end());
    value.assign(decode_result.value.begin(), decode_result.value.end());
  }
};

enum class ReplayState { NEWSTATE, READ, RESIZE, DECODE, MOVE };

enum class ReplayInternalState {
  TRUNCATED,
  CORRUPTED,
  BYTESLESSTHANZERO,
  NOSTATE,
  ENDOFFILE,
  PROCESS
};

struct ReplayResult {
  ReplayInternalState state;
  std::size_t total_buffer_bytes;
  std::size_t consumed_bytes;
  std::size_t pos_offset;
  std::size_t leftover;
};
