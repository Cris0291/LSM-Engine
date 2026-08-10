#include "sstable.h"
#include "lsm_utilities.h"
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <span>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

Sstable::Sstable(std::string path) {
  fd = open(path.data(), O_RDONLY);
  if (fd == -1) {
    throw std::runtime_error("sstable could nto be found");
  }

  struct stat file_info;
  int res{fstat(fd, &file_info)};

  if (res != 0) {
    throw std::runtime_error(
        "something went wrong while reading the sstable file");
  }

  file_size = file_info.st_size;
};

Memtable::Record Sstable::read(std::vector<std::byte> key) {
  std::size_t footer_offset{file_size - FOOTER_SIZE};
  std::vector<std::byte> footer{};
  std::size_t footer_res{};
  ssize_t read_res{};
  std::vector<std::byte> index{};
  std::vector<std::byte> records{};
  std::size_t data_block_size{};

  ReadBlockResult footer_op_res{read_block(footer, FOOTER_SIZE, footer_offset)};
  if (footer_op_res == ReadBlockResult::ERROR) {
    throw std::runtime_error("something went wrong while reading the footer");
  }

  std::uint64_t index_offset{from_n_bytes_little_endian<std::uint64_t, 8>(
      std::span<std::byte>{footer}.subspan(0, 8))};

  std::uint64_t index_size{from_n_bytes_little_endian<std::uint64_t, 8>(
      std::span<std::byte>{footer}.subspan(8, 8))};

  ReadBlockResult index_op_res{read_block(index, index_size, index_offset)};
  if (index_op_res == ReadBlockResult::ERROR) {
    throw std::runtime_error("something went wrong while reading the index");
  }

  std::vector<IndexEntry> index_entries{parse_index(index)};
  std::size_t res{search_entry(index_entries, key)};

  if (res == index_entries.size() - 1) {
    data_block_size = index_entries[res].offset - index_offset;
  } else {
    std::size_t temp{res};
    data_block_size = index_entries[res].offset - index_entries[++temp].offset;
  }

  ReadBlockResult records_op_res{
      read_block(records, data_block_size, index_entries[res].offset)};
  if (records_op_res == ReadBlockResult::ERROR) {
    throw std::runtime_error(
        "something went wrong while reading the data block");
  }
};

std::vector<IndexEntry> Sstable::parse_index(std::vector<std::byte> &index) {
  std::size_t offset{};
  const std::size_t offset_size{8};
  const std::size_t key_size{4};
  IndexEntry index_entry{};
  std::span<std::byte> window;
  std::vector<IndexEntry> res{};

  while (offset < index.size()) {
    std::span<std::byte> offset_window{
        std::span<std::byte>{index}.subspan(offset, offset_size)};
    std::uint64_t off64{
        from_n_bytes_little_endian<std::uint64_t, 8>(offset_window)};
    index_entry.offset = off64;
    offset += offset_size;

    std::span<std::byte> key_len_window{
        std::span<std::byte>{index}.subspan(offset, key_size)};
    std::uint32_t key_len{
        from_n_bytes_little_endian<std::uint32_t, 4>(key_len_window)};
    offset += key_size;

    std::vector<std::byte> key(index.begin() + offset, index.begin() + key_len);
    index_entry.key = key;
    res.push_back(index_entry);
    offset += key_len;
  }

  return res;
};

std::size_t Sstable::search_entry(const std::vector<IndexEntry> &entries,
                                  const std::vector<std::byte> &key) {
  std::size_t low{0};
  std::size_t high{entries.size() - 1};
  std::size_t res{0};

  while (low <= high) {
    std::size_t mid = low + (high - low) / 2;
    int comparison{compare_bytes(entries[mid].key, key)};
    if (comparison <= 0) {
      res = mid;
      low = mid + 1;
    } else {
      high = mid - 1;
    }
  }
  return res;
};

void Sstable::search_records(std::vector<std::byte> &records) {
  std::size_t curr_size{12};
  std::size_t op_size{1};
  std::size_t value_size{4};
  std::uint32_t crc{from_n_bytes_little_endian<std::uint32_t, 4>(
      std::span<std::byte>{records}.subspan(0, 4))};
  std::uint32_t block_size{from_n_bytes_little_endian<std::uint32_t, 4>(
      std::span<std::byte>{records}.subspan(0, 4))};
  std::uint32_t record_count{from_n_bytes_little_endian<std::uint32_t, 4>(
      std::span<std::byte>{records}.subspan(8, 4))};

  while (curr_size < block_size) {
  }
};

ReadBlockResult Sstable::read_block(std::vector<std::byte> &block,
                                    std::uint64_t size, std::uint64_t offset) {
  std::size_t block_result{};
  ssize_t read_result{};

  while (block_result < size) {
    read_result = pread(fd, block.data(), size, offset);
    if (read_result == 0) {
      return ReadBlockResult::GOOD;
    }
    if (read_result < 0) {
      return ReadBlockResult::ERROR;
    }
    block_result += read_result;
  }

  return ReadBlockResult::GOOD;
};
