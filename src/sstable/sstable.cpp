#include "sstable.h"
#include <cstddef>
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

  ReadBlockResult footer_op_res{read_block(footer, FOOTER_SIZE, footer_offset)};
  if (footer_op_res == ReadBlockResult::ERROR) {
    throw std::runtime_error("something went wrong while reading the footer");
  }

  std::uint64_t index_offset{
      from_8_bytes_little_endian(std::span<std::byte>{footer}.subspan(0, 8))};

  std::uint64_t index_size{
      from_8_bytes_little_endian(std::span<std::byte>{footer}.subspan(8, 8))};

  ReadBlockResult index_op_res{read_block(index, index_size, index_offset)};
  if (index_op_res == ReadBlockResult::ERROR) {
    throw std::runtime_error("something went wrong while reading the index");
  }
};

std::vector<IndexEntry> Sstable::parse_index(std::vector<std::byte> &index) {
  std::size_t offset{};
  const std::size_t offset_size{8};
  const std::size_t key_size{4};
  IndexEntry index_entry{};
  std::span<std::byte> window;

  while (offset < index.size()) {
    std::span<std::byte> offset_window{
        std::span<std::byte>{index}.subspan(offset, offset_size)};
    std::uint64_t off64{from_8_bytes_little_endian(offset_window)};
    index_entry.offset = off64;
    offset += offset_size;
  }
};

ReadBlockResult Sstable::read_block(std::vector<std::byte> &block,
                                    std::size_t size, std::size_t offset) {
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

std::uint64_t
Sstable::from_8_bytes_little_endian(std::span<std::byte> bytes_to_copy) {
  std::uint64_t value64 =
      std::to_integer<std::uint64_t>(bytes_to_copy[0]) |
      (std::to_integer<std::uint64_t>(bytes_to_copy[1]) << 8) |
      (std::to_integer<std::uint64_t>(bytes_to_copy[2]) << 16) |
      (std::to_integer<std::uint64_t>(bytes_to_copy[3]) << 24) |
      (std::to_integer<std::uint64_t>(bytes_to_copy[4]) << 32) |
      (std::to_integer<std::uint64_t>(bytes_to_copy[5]) << 40) |
      (std::to_integer<std::uint64_t>(bytes_to_copy[6]) << 48) |
      (std::to_integer<std::uint64_t>(bytes_to_copy[7]) << 52);

  return value64;
};
