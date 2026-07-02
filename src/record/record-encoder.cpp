#include "record-encoder.h"

std::vector<std::byte> RecordEncoder::encode(OperationRecord ops,
                                             std::span<std::byte> keys,
                                             std::span<std::byte> values) {
  std::vector<std::byte> out{};
  if (keys.size_bytes() > UINT32_MAX || values.size_bytes() > UINT32_MAX) {
    throw std::length_error("key or value exceeded maximum encoded value");
  }
  std::size_t keys_size_bytes{keys.size_bytes()};
  std::size_t values_size_bytes{values.size_bytes()};
  std::array<std::uint8_t, 4> keys_bytes{};
  std::array<std::uint8_t, 4> values_bytes{};

  to_4_bytes_little_endian(keys_size_bytes, keys_bytes);
  to_4_bytes_little_endian(values_size_bytes, values_bytes);

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

  uLong crc{crc32(0L, Z_NULL, 0)};
  auto starting_pos{out.data() + 4};
  crc = crc32(crc, reinterpret_cast<const unsigned char *>(starting_pos),
              static_cast<uInt>(total_size - 4));

  std::array<std::uint8_t, 4> crc_bytes{};
  to_4_bytes_little_endian(static_cast<std::size_t>(crc), crc_bytes);
  std::memcpy(out.data(), crc_bytes.data(), crc_bytes.size());
  return out;
};

DecodeResult RecordEncoder::decode(std::span<std::byte> record) {
  std::size_t record_size{record.size_bytes()};
  if (record_size < HEADER_SIZE)
    return {DecodeStatus::TRUNCATED, {}, {}, {}, 0};

  std::uint32_t keys_bytes{from_4_bytes_little_endian(record.subspan(5, 4))};
  std::uint32_t values_bytes{from_4_bytes_little_endian(record.subspan(9, 4))};

  // 64 bit int result so that untrusted keys or values dont overflow
  std::uint64_t needed{static_cast<std::uint64_t>(HEADER_SIZE) + keys_bytes +
                       values_bytes};

  if (record_size < needed) {
    return {DecodeStatus::TRUNCATED, {}, {}, {}, 0};
  }

  uLong crc{crc32(0L, Z_NULL, 0)};
  auto starting_pos{record.data() + CHECKSUM_SIZE};
  crc = crc32(crc, reinterpret_cast<const unsigned char *>(starting_pos),
              static_cast<uInt>(record_size - CHECKSUM_SIZE));

  std::uint32_t crc_value{from_4_bytes_little_endian(record.subspan(0, 4))};
  if (crc_value != crc) {
    return {DecodeStatus::CORRUPTED, {}, {}, {}, 0};
  }

  OperationRecord op{static_cast<OperationRecord>(record[CHECKSUM_SIZE])};
  std::span<std::byte> key{record.subspan(HEADER_SIZE, keys_bytes)};
  std::span<std::byte> value{
      record.subspan(HEADER_SIZE + keys_bytes, values_bytes)};

  return {DecodeStatus::GOOD, op, key, value,
          HEADER_SIZE + keys_bytes + values_bytes};
};

void RecordEncoder::to_4_bytes_little_endian(
    std::size_t value, std::array<std::uint8_t, 4> &bytes) {
  std::uint32_t val32{static_cast<std::uint32_t>(value)};
  bytes[0] = static_cast<std::uint8_t>(val32 & 0xFF);
  bytes[1] = static_cast<std::uint8_t>((val32 >> 8) & 0xFF);
  bytes[2] = static_cast<std::uint8_t>((val32 >> 16) & 0xFF);
  bytes[3] = static_cast<std::uint8_t>((val32) >> 24);
};

std::uint32_t
RecordEncoder::from_4_bytes_little_endian(std::span<std::byte> bytes_to_copy) {
  std::uint32_t val32 =
      std::to_integer<std::uint32_t>(bytes_to_copy[0]) |
      (std::to_integer<std::uint32_t>(bytes_to_copy[1]) << 8) |
      (std::to_integer<std::uint32_t>(bytes_to_copy[2]) << 16) |
      (std::to_integer<std::uint32_t>(bytes_to_copy[3]) << 24);

  return val32;
};
