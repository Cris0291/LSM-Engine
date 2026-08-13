#include "sstable_writer.h"
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
  std::vector<std::byte> buffer(BUFFER_SIZE * 2);

  for (int i{}; i < records.size(); i++) {
    if (curr_pos >= BUFFER_SIZE) {
      buffer.resize(curr_pos);
      data_blocks.push_back(std::move(buffer));
      buffer.clear();
      buffer.resize(BUFFER_SIZE * 2);
    }
    buffer[curr_pos] = static_cast<std::byte>(records[i].op);
    curr_pos += OP_SIZE;
    to_4_bytes_little_endian(records[i].key.size(), curr_pos, buffer);
    curr_pos += KEY_SIZE;
    to_4_bytes_little_endian(records[i].value.size(), curr_pos, buffer);
    curr_pos += VALUE_SIZE;
    std::copy(records[i].key.begin(), records[i].key.end(),
              buffer.begin() + curr_pos);
    curr_pos += records[i].key.size();
    std::copy(records[i].value.begin(), records[i].value.end(),
              buffer.begin() + curr_pos);
    curr_pos += records[i].value.size();
  }
};

void SstableWriter::flush_memtable(Memtable &memtable) {
  std::vector<Memtable::Record> records{memtable.linear_iteration()};

  std::vector<std::byte> buffer(BUFFER_SIZE);
};

void SstableWriter::to_4_bytes_little_endian(std::size_t value,
                                             std::size_t offset,
                                             std::vector<std::byte> &bytes) {
  std::uint32_t value32{static_cast<std::uint32_t>(value)};

  bytes[offset] = static_cast<std::byte>(value32 & 0xFF);
  offset += 1;
  bytes[offset] = static_cast<std::byte>((value32 >> 8) & 0xFF);
  offset += 1;
  bytes[offset] = static_cast<std::byte>((value32 >> 16) & 0xFF);
  offset += 1;
  bytes[offset] = static_cast<std::byte>((value32 >> 24) & 0xFF);
};
