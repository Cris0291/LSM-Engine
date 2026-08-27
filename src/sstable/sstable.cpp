#include "sstable.h"
#include "lsm_utilities.h"
#include "memtable.h"
#include "sstable_operation.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <span>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

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

Sstable::~Sstable() { close(fd); };

std::vector<RecordSstable> Sstable::linera_iteration() {
  std::size_t footer_offset{file_size - FOOTER_SIZE};
  std::vector<std::byte> footer{FOOTER_SIZE};
  std::vector<RecordSstable> res;
  std::size_t curr_size{};
  std::size_t block_size_offset{4};
  std::size_t start_pos{HEADER_SIZE};
  std::size_t block_size{};

  ReadBlockResult footer_op_res{read_block(footer, FOOTER_SIZE, footer_offset)};
  if (footer_op_res == ReadBlockResult::ERROR) {
    throw std::runtime_error("something went wrong while reading the footer");
  }

  std::uint64_t index_size{from_n_bytes_little_endian<std::uint64_t, 8>(
      std::span<std::byte>{footer}.subspan(8, 8))};

  std::size_t total_file_size{file_size - index_size - FOOTER_SIZE};

  std::vector<std::byte> blocks{total_file_size};

  ReadBlockResult blocks_op_res{read_block(blocks, total_file_size, 0)};
  if (blocks_op_res == ReadBlockResult::ERROR) {
    throw std::runtime_error(
        "something went wrong while reading the blocks of the file");
  }

  std::cerr << "file size in reader : " << file_size
            << "index size : " << index_size
            << " data block size : " << total_file_size << "\n";

  while (curr_size < total_file_size) {
    std::uint32_t block_size_with_header{
        from_n_bytes_little_endian<std::uint32_t, 4>(
            std::span<std::byte>{blocks}.subspan(block_size_offset, 4))};
    block_size = (block_size_with_header - HEADER_SIZE);

    parse_blocks(blocks, res, start_pos + block_size, start_pos);
    curr_size += block_size_with_header;
    block_size_offset += block_size_with_header;
    start_pos += block_size_with_header;
  }

  return res;
};

void Sstable::parse_blocks(std::vector<std::byte> &records,
                           std::vector<RecordSstable> &parsed_records,
                           std::size_t size, std::size_t curr_size) {
  OperationRecord op{};
  std::uint32_t key_len{};
  std::uint32_t value_len{};
  RecordSstable p_record{};

  while (curr_size < size) {
    op = static_cast<OperationRecord>(records[curr_size]);
    curr_size += OP_SIZE;
    std::cerr << "1" << "\n";

    key_len = from_n_bytes_little_endian<std::uint32_t, 4>(
        std::span<std::byte>{records}.subspan(curr_size, KEY_VALUE_SIZE));
    curr_size += KEY_VALUE_SIZE;
    std::cerr << "2" << "\n";

    value_len = from_n_bytes_little_endian<std::uint32_t, 4>(
        std::span<std::byte>{records}.subspan(curr_size, KEY_VALUE_SIZE));
    curr_size += KEY_VALUE_SIZE;
    std::cerr << "3" << "size : " << size << "cur size : " << curr_size << "\n";

    p_record.key = std::vector<std::byte>(
        records.begin() + curr_size, records.begin() + curr_size + key_len);
    curr_size += key_len;
    std::cerr << "4" << "\n";

    p_record.value = std::vector<std::byte>(
        records.begin() + curr_size, records.begin() + curr_size + value_len);
    curr_size += value_len;
    std::cerr << "5" << "\n";

    p_record.op = op;
    std::cerr << "6" << "\n";

    parsed_records.push_back(std::move(p_record));
    std::cerr << "7" << "\n";
  }
};

std::optional<RecordSstable> Sstable::read(std::vector<std::byte> key) {
  std::size_t footer_offset{file_size - FOOTER_SIZE};
  std::vector<std::byte> footer{FOOTER_SIZE};
  std::size_t footer_res{};
  ssize_t read_res{};
  std::size_t data_block_size{};

  ReadBlockResult footer_op_res{read_block(footer, FOOTER_SIZE, footer_offset)};
  if (footer_op_res == ReadBlockResult::ERROR) {
    throw std::runtime_error("something went wrong while reading the footer");
  }

  std::uint64_t index_offset{from_n_bytes_little_endian<std::uint64_t, 8>(
      std::span<std::byte>{footer}.subspan(0, 8))};

  std::uint64_t index_size{from_n_bytes_little_endian<std::uint64_t, 8>(
      std::span<std::byte>{footer}.subspan(8, 8))};

  std::vector<std::byte> index{index_size};

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

  std::vector<std::byte> records{data_block_size};

  ReadBlockResult records_op_res{
      read_block(records, data_block_size, index_entries[res].offset)};
  if (records_op_res == ReadBlockResult::ERROR) {
    throw std::runtime_error(
        "something went wrong while reading the data block");
  }

  auto record_res{search_records(records, key)};
  return record_res;
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

std::optional<RecordSstable>
Sstable::search_records(std::vector<std::byte> &records,
                        std::span<std::byte> key) {
  std::size_t curr_size{12};
  std::uint32_t crc{from_n_bytes_little_endian<std::uint32_t, 4>(
      std::span<std::byte>{records}.subspan(0, 4))};
  std::uint32_t block_size{from_n_bytes_little_endian<std::uint32_t, 4>(
      std::span<std::byte>{records}.subspan(0, 4))};
  std::uint32_t record_count{from_n_bytes_little_endian<std::uint32_t, 4>(
      std::span<std::byte>{records}.subspan(8, 4))};

  OperationRecord op{};
  std::uint32_t key_len{};
  std::uint32_t value_len{};
  int comparison_res{-1};

  std::size_t record_offset{};

  while (curr_size < block_size) {
    record_offset = curr_size;

    curr_size += OP_SIZE;
    key_len = from_n_bytes_little_endian<std::uint32_t, 4>(
        std::span<std::byte>{records}.subspan(curr_size, KEY_VALUE_SIZE));
    curr_size += KEY_VALUE_SIZE;
    value_len = from_n_bytes_little_endian<std::uint32_t, 4>(
        std::span<std::byte>{records}.subspan(curr_size, KEY_VALUE_SIZE));
    curr_size += KEY_VALUE_SIZE;
    std::span<std::byte> record_key{
        std::span<std::byte>{records}.subspan(curr_size, key_len)};

    comparison_res = compare_bytes(record_key, key);
    curr_size += key_len;
    curr_size += value_len;

    if (comparison_res == 0)
      break;
  }

  if (comparison_res == 0) {
    RecordSstable record{};
    record.op = static_cast<OperationRecord>(records[record_offset]);
    record_offset += OP_SIZE;
    record_offset += KEY_VALUE_SIZE * 2;
    record.key =
        std::vector<std::byte>(records.begin() + record_offset,
                               records.begin() + record_offset + key_len);
    record_offset += key_len;
    record.value =
        std::vector<std::byte>(records.begin() + record_offset,
                               records.begin() + record_offset + value_len);
    return record;
  }

  return {};
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
