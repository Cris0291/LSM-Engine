#include "record-encoder.h"

std::span<std::byte> encode(OperationRecord ops, std::span<std::byte> keys,
                            std::span<std::byte> values) {
  std::size_t keys_size_bytes{keys.size_bytes()};
  std::size_t values_size_bytes{values.size_bytes()};
};

std::array<std::uint8_t, 4> RecordEncoder::to_4_bytes(std::size_t value) {
  std::uint32_t val32{static_cast<std::uint32_t>(value)};
}
