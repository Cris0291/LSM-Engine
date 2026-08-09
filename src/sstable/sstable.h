#pragma once

#include "memtable.h"
#include "sstable_operation.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

class Sstable {
private:
  static constexpr std::size_t FOOTER_SIZE{20};
  int fd;
  std::size_t file_size;
  std::uint64_t from_8_bytes_little_endian(std::span<std::byte> byes_to_copy);
  ReadBlockResult read_block(std::vector<std::byte> &block, std::size_t size,
                             std::size_t offset);
  std::vector<IndexEntry> parse_index(std::vector<std::byte> &index);

public:
  Sstable(std::string path);
  Memtable::Record read(std::vector<std::byte> key);
};
