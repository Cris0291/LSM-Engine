#include "sstable.h"
#include "lsm_utilities.h"
#include "operation.h"
#include "sstable_operation.h"
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

Sstable::Sstable(std::string path, Record _min_key, Record _max_key)
    : max_key(_max_key), min_key(_min_key) {
  fd = open(path.data(), O_RDONLY);
  if (fd == -1) {
    throw std::runtime_error("sstable could not be found");
  }

  struct stat file_info;
  int res{fstat(fd, &file_info)};

  if (res != 0) {
    throw std::runtime_error(
        "something went wrong while reading the sstable file");
  }

  file_size = file_info.st_size;

  std::size_t footer_offset{file_size - FOOTER_SIZE};
  std::vector<std::byte> footer{FOOTER_SIZE};
  std::size_t footer_res{};

  ReadBlockResult footer_op_res{read_block(footer, FOOTER_SIZE, footer_offset)};
  if (footer_op_res == ReadBlockResult::ERROR) {
    throw std::runtime_error("something went wrong while reading the footer");
  }

  std::uint64_t _index_offset{from_n_bytes_little_endian<std::uint64_t, 8>(
      std::span<std::byte>{footer}.subspan(0, 8))};

  std::uint64_t _index_size{from_n_bytes_little_endian<std::uint64_t, 8>(
      std::span<std::byte>{footer}.subspan(8, 8))};

  std::vector<std::byte> index{_index_size};

  ReadBlockResult index_op_res{read_block(index, _index_size, _index_offset)};
  if (index_op_res == ReadBlockResult::ERROR) {
    throw std::runtime_error("something went wrong while reading the index");
  }

  std::vector<IndexEntry> _index_entries{parse_index(index)};

  index_entries = std::move(_index_entries);
  index_size = _index_size;
  index_offset = _index_offset;

  sstable_path = path;
};

Sstable::~Sstable() { close(fd); };

std::vector<Record> Sstable::linera_iteration() {
  std::vector<Record> res;
  std::size_t curr_size{};
  std::size_t block_size_offset{4};
  std::size_t start_pos{HEADER_SIZE};
  std::size_t block_size{};

  std::size_t total_file_size{file_size - index_size - FOOTER_SIZE};

  std::vector<std::byte> blocks{total_file_size};

  ReadBlockResult blocks_op_res{read_block(blocks, total_file_size, 0)};
  if (blocks_op_res == ReadBlockResult::ERROR) {
    throw std::runtime_error(
        "something went wrong while reading the blocks of the file");
  }

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
                           std::vector<Record> &parsed_records,
                           std::size_t size, std::size_t curr_size) {
  OperationRecord op{};
  std::uint32_t key_len{};
  std::uint32_t value_len{};
  Record p_record{};

  while (curr_size < size) {
    op = static_cast<OperationRecord>(records[curr_size]);
    curr_size += OP_SIZE;

    key_len = from_n_bytes_little_endian<std::uint32_t, 4>(
        std::span<std::byte>{records}.subspan(curr_size, KEY_VALUE_SIZE));
    curr_size += KEY_VALUE_SIZE;

    value_len = from_n_bytes_little_endian<std::uint32_t, 4>(
        std::span<std::byte>{records}.subspan(curr_size, KEY_VALUE_SIZE));
    curr_size += KEY_VALUE_SIZE;

    p_record.key = std::vector<std::byte>(
        records.begin() + curr_size, records.begin() + curr_size + key_len);
    curr_size += key_len;

    p_record.value = std::vector<std::byte>(
        records.begin() + curr_size, records.begin() + curr_size + value_len);
    curr_size += value_len;

    p_record.op = op;

    parsed_records.push_back(std::move(p_record));
  }
};

std::optional<Record> Sstable::read(std::vector<std::byte> key) {
  std::size_t data_block_size{};

  std::size_t res{search_entry(index_entries, key)};

  if (res == index_entries.size() - 1) {
    data_block_size = index_offset - index_entries[res].offset;
  } else {
    std::size_t temp{res};
    data_block_size = index_entries[++temp].offset - index_entries[res].offset;
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

    std::vector<std::byte> key(index.begin() + offset,
                               index.begin() + offset + key_len);

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
    if (low == 0 && high == 0)
      return mid;
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

std::optional<Record> Sstable::search_records(std::vector<std::byte> &records,
                                              std::span<std::byte> key) {
  std::size_t curr_size{12};
  std::uint32_t crc{from_n_bytes_little_endian<std::uint32_t, 4>(
      std::span<std::byte>{records}.subspan(0, 4))};
  std::uint32_t block_size{from_n_bytes_little_endian<std::uint32_t, 4>(
      std::span<std::byte>{records}.subspan(4, 4))};
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
    Record record{};
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
      std::cerr << "pread failed " << "fd" << fd << "errno" << errno
                << "size :" << size << "offset: " << offset
                << "fiel size: " << file_size << strerror(errno) << "\n";
      return ReadBlockResult::ERROR;
    }
    block_result += read_result;
  }

  return ReadBlockResult::GOOD;
};

Sstable::Iterator::Iterator(std::shared_ptr<Sstable> _parent, int n,
                            std::size_t size)
    : parent(_parent), curr_block(n), block_size(size) {}

Sstable::Iterator::Iterator(const Iterator &other) {
  parent = other.parent;
  curr_block = other.curr_block;
  block_size = other.block_size;
  block_offset = other.block_offset;
  buffer_pos = other.buffer_pos;
  records_buffer = other.records_buffer;
}

Sstable::Iterator Sstable::begin() {
  // here we should handle the edge case in which there
  // is only one block instead of seaching for the difference of block 0 againt
  // block 1 the difference between block 0 and index must be used
  std::size_t block_0_size{index_entries.size() > 1 ? index_entries[1].offset
                                                    : index_offset};
  Iterator it{Iterator(shared_from_this(), 0, block_0_size)};
  it.fill_buffer(it.parent);
  return it;
}

Sstable::Iterator Sstable::end() {
  Iterator it{Iterator(shared_from_this(), index_entries.size(), INT_MAX)};
  return it;
}

Sstable::Iterator &Sstable::Iterator::operator++() {
  buffer_pos += 1;
  if (trigger_fill()) {
    curr_block += 1;
    if (curr_block == parent.get()->index_entries.size()) {
      buffer_pos = 0;
      records_buffer.clear();
      return *this;
    }
    block_offset = parent.get()->index_entries[curr_block].offset;
    std::size_t next{curr_block + 1};
    block_size = next < parent.get()->index_entries.size()
                     ? parent.get()->index_entries[next].offset - block_offset
                     : parent.get()->index_offset - block_offset;
    fill_buffer(parent);
    buffer_pos = 0;
  }

  return *this;
}

Sstable::Iterator Sstable::Iterator::operator++(int) {
  Iterator it{Iterator(*this)};

  buffer_pos += 1;
  if (trigger_fill()) {
    curr_block += 1;
    if (curr_block == parent.get()->index_entries.size()) {
      buffer_pos = 0;
      records_buffer.clear();
      return *this;
    }
    block_offset = parent.get()->index_entries[curr_block].offset;
    std::size_t next{curr_block + 1};
    block_size = next < parent.get()->index_entries.size()
                     ? parent.get()->index_entries[next].offset - block_offset
                     : parent.get()->index_offset - block_offset;
    fill_buffer(parent);
    buffer_pos = 0;
  }

  return it;
}

Record Sstable::Iterator::operator*() { return records_buffer[buffer_pos]; }

bool Sstable::Iterator::operator==(const Iterator &other) const {
  return (parent == other.parent && curr_block == other.curr_block &&
          buffer_pos == other.buffer_pos);
}

bool Sstable::Iterator::operator!=(const Iterator &other) const {
  return !(*this == other);
}

bool Sstable::Iterator::trigger_fill() {
  return records_buffer.size() == buffer_pos;
}

void Sstable::Iterator::fill_buffer(std::shared_ptr<Sstable> parent) {
  // there is race condition here in case multiple operations try to
  // access the shared state i case the iterator could be copied
  if (records_buffer.size() > 0) {
    records_buffer.clear();
  }

  std::vector<std::byte> buffer(block_size);
  ReadBlockResult res{
      parent.get()->read_block(buffer, block_size, block_offset)};

  if (res == ReadBlockResult::ERROR) {
    throw std::runtime_error("iterator failed");
  }

  std::size_t total_size_without_header{buffer.size() - HEADER_SIZE};
  parent->parse_blocks(buffer, records_buffer, total_size_without_header,
                       HEADER_SIZE);
}
