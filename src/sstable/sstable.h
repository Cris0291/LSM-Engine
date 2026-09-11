#pragma once

#include "sstable_operation.h"
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

class Sstable {
private:
  static constexpr std::size_t FOOTER_SIZE{20};
  static constexpr std::size_t HEADER_SIZE{12};
  static constexpr std::size_t OP_SIZE{1};
  static constexpr std::size_t KEY_VALUE_SIZE{4};
  int fd;
  std::size_t file_size;
  std::vector<IndexEntry> index_entries;
  std::uint64_t index_size;
  std::uint64_t index_offset;
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
  std::optional<RecordSstable> search_records(std::vector<std::byte> &records,
                                              std::span<std::byte> key);
  void parse_blocks(std::vector<std::byte> &records,
                    std::vector<RecordSstable> &parsed_records,
                    std::size_t size, std::size_t curr_size);

public:
  std::string sstable_path;
  Sstable(std::string path);
  ~Sstable();
  std::optional<RecordSstable> read(std::vector<std::byte> key);
  std::vector<RecordSstable> linera_iteration();
};
