#include "record-encoder.h"

std::span<std::byte> RecordEncoder::encode(OperationRecord ops,
                                           std::span<std::byte> keys,
                                           std::span<std::byte> values) {
  std::vector<std::byte> out{};
  std::size_t keys_size_bytes{keys.size_bytes()};
  std::size_t values_size_bytes{values.size_bytes()};
  std::array<std::uint8_t, 4> keys_bytes{};
  std::array<std::uint8_t, 4> values_bytes{};
  if constexpr (std::endian::native == std::endian::big) {
    to_4_bytes_little_endian(keys_size_bytes, keys_bytes);
    to_4_bytes_little_endian(values_size_bytes, values_bytes);
  } else {
  }

  std::byte op_byte{static_cast<std::byte>(ops)};

  std::size_t offset{4};
  std::size_t total_size{offset + keys.size() + values.size() +
                         keys_bytes.size() + values_bytes.size() + 1};

  out.resize(total_size);

  auto copy_bytes = [&](auto src) {
    std::memcpy(out.data() + offset, src.data(), src.size());
    offset += src.size();
  };

  out[offset] = op_byte;
  ++offset;

  copy_bytes(keys_bytes);
  copy_bytes(values_bytes);
  copy_bytes(static_cast<std::span<std::byte>>(keys));
  copy_bytes(static_cast<std::span<std::byte>>(values));
};

void RecordEncoder::to_4_bytes_little_endian(
    std::size_t value, std::array<std::uint8_t, 4> &bytes) {
  std::uint32_t val32{static_cast<std::uint32_t>(value)};
  bytes[0] = static_cast<std::uint8_t>(val32 & 0xFF);
  bytes[1] = static_cast<std::uint8_t>((val32 >> 8) & 0xFF);
  bytes[2] = static_cast<std::uint8_t>((val32 >> 16) & 0xFF);
  bytes[3] = static_cast<std::uint8_t>((val32) >> 24);
};
