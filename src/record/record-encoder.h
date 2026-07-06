#include "operation.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <zlib.h>

class RecordEncoder {
private:
  static constexpr std::size_t HEADER_SIZE{13};
  static constexpr std::size_t CHECKSUM_SIZE{4};
  static constexpr std::size_t OP_SIZE{1};
  static constexpr std::size_t KEYS_OR_VALUE_LENGTH{4};
  static void to_4_bytes_little_endian(std::size_t value,
                                       std::array<std::uint8_t, 4> &bytes);
  static std::uint32_t
  from_4_bytes_little_endian(std::span<std::byte> bytes_to_order);

public:
  static std::vector<std::byte>
  encode(OperationRecord ops, std::span<std::byte> keys, std::span<std::byte>);
  static DecodeResult decode(std::span<std::byte> record);
};
