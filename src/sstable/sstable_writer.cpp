#include "sstable_writer.h"
#include "zlib.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <stdexcept>
#include <sys/uio.h>
#include <unistd.h>

SstableWriter::SstableWriter(std::string path) {
  fd = open(path.data(), O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    throw std::runtime_error("could not create the file for sstable write");
  }

  std::filesystem::path p{path};
  dir_path = p.parent_path().string();
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

  for (auto &record : records) {

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
      set_header(curr_size, record_count, buffer);
      data_blocks.emplace_back(std::move(buffer));
      buffer.reserve(BUFFER_SIZE);
      buffer.resize(HEADER_SIZE);

      std::get<0>(index) = curr_size;
      index_blocks.push_back(std::move(index));

      record_count = 0;
      curr_size = HEADER_SIZE;
    }
  }

  if (buffer.size() > HEADER_SIZE) {
    set_header(curr_size, record_count, buffer);
    data_blocks.emplace_back(std::move(buffer));

    std::get<0>(index) = curr_size;
    index_blocks.push_back(std::move(index));
  }
};

std::pair<std::uint64_t, std::uint64_t> SstableWriter::create_index(
    std::vector<std::vector<std::byte>> &index,
    std::vector<std::tuple<std::size_t, std::size_t, std::vector<std::byte>>>
        &index_blocks) {
  std::vector<std::byte> buffer;
  std::uint64_t accumulation_offset{};
  std::uint64_t total_size{};

  for (int i{}; i < index_blocks.size(); i++) {
    std::size_t offset{std::get<0>(index_blocks[i])};
    to_8_bytes_little_endian(accumulation_offset, buffer);
    total_size += INDEX_OFFSET;

    std::size_t key_len{std::get<1>(index_blocks[i])};
    to_4_bytes_little_endian(key_len, buffer);
    total_size += KEY_SIZE;

    std::vector<std::byte> &key{std::get<2>(index_blocks[i])};
    buffer.insert(buffer.end(), key.begin(), key.end());
    total_size += key_len;

    accumulation_offset += offset;
  }

  return {accumulation_offset, total_size};
};

void SstableWriter::write_sstable(std::vector<std::vector<std::byte>> &data,
                                  std::vector<std::vector<std::byte>> &index,
                                  std::vector<std::byte> &footer) {
  std::vector<iovec> iovecs;
  long max_iovecs{sysconf(_SC_IOV_MAX)};
  std::size_t written_size{};

  for (auto &block : data) {
    iovecs.push_back({.iov_base = block.data(), .iov_len = block.size()});
  }

  for (auto &block : index) {
    iovecs.push_back({.iov_base = block.data(), .iov_len = block.size()});
  }

  iovecs.push_back({.iov_base = footer.data(), .iov_len = footer.size()});

  if (iovecs.size() > max_iovecs) {
    for (; written_size < iovecs.size(); written_size += max_iovecs) {
      writev(fd, iovecs.data() + written_size, max_iovecs);
      if ((iovecs.size() - written_size) < max_iovecs) {
        break;
      }
    }

    if (written_size < iovecs.size()) {
      writev(fd, iovecs.data() + written_size, iovecs.size() - written_size);
    }
  } else {
    writev(fd, iovecs.data(), iovecs.size());
  }

  fsync(fd);
};

void SstableWriter::flush_memtable(Memtable &memtable) {
  std::vector<Memtable::Record> records{memtable.linear_iteration()};

  std::vector<std::vector<std::byte>> data_blocks;
  std::vector<std::vector<std::byte>> index;
  std::vector<std::tuple<std::size_t, std::size_t, std::vector<std::byte>>>
      index_blocks;
  std::vector<std::byte> footer;

  create_blocks(records, data_blocks, index_blocks);
  auto index_res{create_index(index, index_blocks)};

  std::uint64_t index_offset{index_res.first};
  std::uint64_t index_size{index_res.second};

  to_8_bytes_little_endian(index_offset, footer);
  to_8_bytes_little_endian(index_size, footer);
  to_4_bytes_little_endian(MAGIC, footer);

  write_sstable(data_blocks, index, footer);
};

void SstableWriter::to_4_bytes_little_endian(std::size_t value,
                                             std::vector<std::byte> &bytes) {
  std::uint32_t value32{static_cast<std::uint32_t>(value)};

  bytes.push_back(static_cast<std::byte>(value32 & 0xFF));
  bytes.push_back(static_cast<std::byte>((value32 >> 8) & 0xFF));
  bytes.push_back(static_cast<std::byte>((value32 >> 16) & 0xFF));
  bytes.push_back(static_cast<std::byte>((value32 >> 24) & 0xFF));
};

void SstableWriter::to_8_bytes_little_endian(std::int64_t value64,
                                             std::vector<std::byte> &bytes) {

  bytes.push_back(static_cast<std::byte>(value64 & 0xFF));
  bytes.push_back(static_cast<std::byte>((value64 >> 8) & 0xFF));
  bytes.push_back(static_cast<std::byte>((value64 >> 16) & 0xFF));
  bytes.push_back(static_cast<std::byte>((value64 >> 24) & 0xFF));
  bytes.push_back(static_cast<std::byte>((value64 >> 32) & 0xFF));
  bytes.push_back(static_cast<std::byte>((value64 >> 40) & 0xFF));
  bytes.push_back(static_cast<std::byte>((value64 >> 48) & 0xFF));
  bytes.push_back(static_cast<std::byte>(value64 >> 56));
};

void SstableWriter::to_4_bytes_little_endian(
    std::size_t value, std::array<std::uint8_t, 4> &bytes) {
  std::uint32_t val32{static_cast<std::uint32_t>(value)};
  bytes[0] = static_cast<std::uint8_t>(val32 & 0xFF);
  bytes[1] = static_cast<std::uint8_t>((val32 >> 8) & 0xFF);
  bytes[2] = static_cast<std::uint8_t>((val32 >> 16) & 0xFF);
  bytes[3] = static_cast<std::uint8_t>((val32) >> 24);
};

void SstableWriter::set_header(std::size_t size, std::size_t num_records,
                               std::vector<std::byte> &buffer) {
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
};
