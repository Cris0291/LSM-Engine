#include "memtable.h"
#include <cstddef>
#include <string>
#include <vector>

class SstableWriter {
private:
  static constexpr std::size_t BUFFER_SIZE{4096};
  static constexpr std::size_t HEADER_SIZE{12};
  static constexpr std::size_t OP_SIZE{1};
  static constexpr std::size_t KEY_SIZE{4};
  static constexpr std::size_t VALUE_SIZE{4};
  int fd;
  void create_data_blocks(std::vector<Memtable::Record> &records,
                          std::vector<std::vector<std::byte>> &data_blocks);
  void to_4_bytes_little_endian(std::size_t value, std::size_t offset,
                                std::vector<std::byte> &bytes);

public:
  SstableWriter(std::string path);
  void flush_memtable(Memtable &memtable);
};
