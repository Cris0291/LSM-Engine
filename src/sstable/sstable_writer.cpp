#include "sstable_writer.h"
#include "zlib.h"
#include <algorithm>
#include <fcntl.h>
#include <stdexcept>

SstableWriter::SstableWriter(std::string path) {
  fd = open(path.data(), O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    throw std::runtime_error("could not cretae the file for sstable write");
  }
};

void SstableWriter::create_data_blocks(
    std::vector<Memtable::Record> &records,
    std::vector<std::vector<std::byte>> &data_blocks) {
  std::size_t curr_pos{HEADER_SIZE};
  std::vector<std::byte> buffer;
  buffer.reserve(BUFFER_SIZE);
  buffer.resize(HEADER_SIZE);

  for (int i{}; i < records.size(); i++) {
    if (curr_pos >= BUFFER_SIZE) {
      uLong crc{crc32(0L, Z_NULL, 0)};
      auto starting_pos{buffer.data() + HEADER_SIZE};
      crc = crc32(crc, reinterpret_cast<const unsigned char *>(starting_pos),
                  static_cast<uInt>(buffer.size() - HEADER_SIZE));

      data_blocks.emplace_back(std::move(buffer));
      buffer.reserve(BUFFER_SIZE);
      buffer.resize(HEADER_SIZE);
    }
    buffer.push_back(static_cast<std::byte>(records[i].op));
    curr_pos += OP_SIZE;
    to_4_bytes_little_endian(records[i].key.size(), buffer);
    curr_pos += KEY_SIZE;
    to_4_bytes_little_endian(records[i].value.size(), buffer);
    curr_pos += VALUE_SIZE;
    buffer.insert(buffer.end(), records[i].key.begin(), records[i].key.end());
    curr_pos += records[i].key.size();
    buffer.insert(buffer.end(), records[i].value.begin(),
                  records[i].value.end());
    curr_pos += records[i].value.size();
  }
};

void SstableWriter::flush_memtable(Memtable &memtable) {
  std::vector<Memtable::Record> records{memtable.linear_iteration()};

  std::vector<std::byte> buffer(BUFFER_SIZE);
};

void SstableWriter::to_4_bytes_little_endian(std::size_t value,
                                             std::vector<std::byte> &bytes) {
  std::uint32_t value32{static_cast<std::uint32_t>(value)};

  bytes.push_back(static_cast<std::byte>(value32 & 0xFF));
  bytes.push_back(static_cast<std::byte>((value32 >> 8) & 0xFF));
  bytes.push_back(static_cast<std::byte>((value32 >> 16) & 0xFF));
  bytes.push_back(static_cast<std::byte>((value32 >> 24) & 0xFF));
};

void SstableWriter::to_4_bytes_little_endian(
    std::size_t value, std::array<std::uint8_t, 4> &bytes) {
  std::uint32_t val32{static_cast<std::uint32_t>(value)};
  bytes[0] = static_cast<std::uint8_t>(val32 & 0xFF);
  bytes[1] = static_cast<std::uint8_t>((val32 >> 8) & 0xFF);
  bytes[2] = static_cast<std::uint8_t>((val32 >> 16) & 0xFF);
  bytes[3] = static_cast<std::uint8_t>((val32) >> 24);
};
