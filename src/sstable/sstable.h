#pragma once

#include "memtable.h"
#include "sstable_operation.h"
#include <cstddef>
#include <span>
#include <string>
#include <vector>

class Sstable {
private:
  static constexpr std::size_t FOOTER_SIZE{20};
  int fd;
  std::size_t file_size;
  template <typename T, std::size_t N>
  T from_n_bytes_little_endian(std::span<std::byte> bytes_to_copy) {
    T value_n{T{}};

    for (int i{}; i < N; i++) {
      value_n |= (std::to_integer<T>(bytes_to_copy[i]) << (i * 8));
    }

    return value_n;
  }
  ReadBlockResult read_block(std::vector<std::byte> &block, std::size_t size,
                             std::size_t offset);
  std::vector<IndexEntry> parse_index(std::vector<std::byte> &index);
  std::size_t search_entry(const std::vector<IndexEntry> &entries,
                           const std::vector<std::byte> &key);
  void search_records(std::vector<std::byte> &records);

public:
  Sstable(std::string path);
  Memtable::Record read(std::vector<std::byte> key);
};
