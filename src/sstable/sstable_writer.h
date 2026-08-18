#include "memtable.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

class SstableWriter {
private:
  static constexpr std::size_t BUFFER_SIZE{4096};
  static constexpr std::size_t HEADER_SIZE{12};
  static constexpr std::size_t OP_SIZE{1};
  static constexpr std::size_t KEY_SIZE{4};
  static constexpr std::size_t VALUE_SIZE{4};
  static constexpr std::size_t CRC_SIZE{4};
  static constexpr std::size_t BLOCK_SIZE{4};
  int fd;
  void create_blocks(
      std::vector<Memtable::Record> &records,
      std::vector<std::vector<std::byte>> &data_blocks,
      std::vector<std::tuple<std::size_t, std::size_t, std::vector<std::byte>>>
          &index_blocks);
  void to_4_bytes_little_endian(std::size_t value,
                                std::vector<std::byte> &bytes);
  void to_8_bytes_little_endian(std::int64_t value64,
                                std::vector<std::byte> &bytes);
  void to_4_bytes_little_endian(std::size_t value,
                                std::array<std::uint8_t, 4> &bytes);
  void set_header(std::size_t size, std::size_t num_records,
                  std::vector<std::byte> &buffer);
  std::size_t create_index(
      std::vector<std::vector<std::byte>> &index,
      std::vector<std::tuple<std::size_t, std::size_t, std::vector<std::byte>>>
          &index_blocks);

public:
  SstableWriter(std::string path);
  void flush_memtable(Memtable &memtable);
};
