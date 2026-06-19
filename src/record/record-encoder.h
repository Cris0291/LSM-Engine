#include "operation.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>
#include <zlib.h>

class RecordEncoder {
private:
  void to_4_bytes_little_endian(std::size_t value,
                                std::array<std::uint8_t, 4> &bytes);

public:
  std::span<std::byte> encode(OperationRecord ops, std::span<std::byte> keys,
                              std::span<std::byte>);
  void decode(OperationRecord ops, std::span<std::byte> record);
};
