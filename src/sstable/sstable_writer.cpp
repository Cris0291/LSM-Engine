#include "sstable_writer.h"
#include "zlib.h"
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>

SstableWriter::SstableWriter(std::string path) {
  fd = open(path.data(), O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    throw std::runtime_error("could not create the file for sstable write");
  }
};

void SstableWriter::create_blocks(
    std::vector<Memtable::Record> &records,
    std::vector<std::vector<std::byte>> &data_blocks,
    std::vector<std::tuple<std::size_t, std::size_t, std::vector<std::byte>>>
        &index_blocks) {
  std::size_t curr_size{HEADER_SIZE};
  std::vector<std::byte> buffer;
  buffer.reserve(BUFFER_SIZE);
  buffer.resize(HEADER_SIZE);
  std::tuple<std::size_t, std::size_t, std::vector<std::byte>> index;
  std::size_t record_count{};
  bool is_first_record{true};
  int is_first_offset{true};

  for (auto record : records) {

    buffer.push_back(static_cast<std::byte>(record.op));
    curr_size += OP_SIZE;
    to_4_bytes_little_endian(record.key.size(), buffer);
    curr_size += KEY_SIZE;
    to_4_bytes_little_endian(record.value.size(), buffer);
    curr_size += VALUE_SIZE;
    buffer.insert(buffer.end(), record.key.begin(), record.key.end());
    curr_size += record.key.size();
    buffer.insert(buffer.end(), record.value.begin(), record.value.end());
    curr_size += record.value.size();

    record_count += 1;

    if (is_first_record) {
      std::get<1>(index) = record.key.size();
      std::get<2>(index) = record.key;
      is_first_record = false;
    }

    if (curr_size >= BUFFER_SIZE) {
      set_header(curr_size, record_count, buffer, data_blocks);

      std::get<0>(index) = (!is_first_offset ? curr_size : 0);
      index_blocks.push_back(std::move(index));
      is_first_record = true;
      is_first_offset = false;

      record_count = 0;
      curr_size = HEADER_SIZE;
    }
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

void SstableWriter::set_header(
    std::size_t size, std::size_t num_records, std::vector<std::byte> &buffer,
    std::vector<std::vector<std::byte>> &data_blocks) {
  uLong crc{crc32(0L, Z_NULL, 0)};
  auto starting_pos{buffer.data() + HEADER_SIZE};
  crc = crc32(crc, reinterpret_cast<const unsigned char *>(starting_pos),
              static_cast<uInt>(buffer.size() - HEADER_SIZE));
  std::array<std::uint8_t, 4> arr_crc_bytes{};
  to_4_bytes_little_endian(static_cast<std::size_t>(crc), arr_crc_bytes);
  memcpy(buffer.data(), arr_crc_bytes.data(), arr_crc_bytes.size());

  std::array<std::uint8_t, 4> arr_block_bytes{};
  to_4_bytes_little_endian(static_cast<std::size_t>(size), arr_block_bytes);
  memcpy(buffer.data() + CRC_SIZE, arr_block_bytes.data(),
         arr_block_bytes.size());

  std::array<std::uint8_t, 4> arr_record_count_bytes{};
  to_4_bytes_little_endian(num_records, arr_record_count_bytes);
  auto start_pos{buffer.data() + (CRC_SIZE + BLOCK_SIZE)};
  memcpy(start_pos, arr_record_count_bytes.data(),
         arr_record_count_bytes.size());

  data_blocks.emplace_back(std::move(buffer));
  buffer.reserve(BUFFER_SIZE);
  buffer.resize(HEADER_SIZE);
};
