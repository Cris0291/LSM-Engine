#include "memtable.h"
#include <cstddef>
#include <string>
#include <vector>

class SstableWriter {
private:
  static constexpr std::size_t BUFFER_SIZE{4096};
  static constexpr std::size_t HEADER_SIZE{12};
  int fd;
  void fill_buffer(std::vector<Memtable::Record> &records,
                   std::vector<std::byte> &buffer);

public:
  SstableWriter(std::string path);
  void flush_memtable(Memtable &memtable);
};
