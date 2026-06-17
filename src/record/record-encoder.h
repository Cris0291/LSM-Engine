#include "operation.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <zlib.h>

class RecordEncoder {
private:
  std::array<std::uint8_t, 4> to_4_bytes(std::size_t value);

public:
  std::span<std::byte> encode(OperationRecord ops, std::span<std::byte> keys,
                              std::span<std::byte>);
  void decode(OperationRecord ops, std::span<std::byte> record);
};
