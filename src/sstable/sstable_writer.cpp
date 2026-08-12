#include "sstable_writer.h"
#include <fcntl.h>
#include <stdexcept>

SstableWriter::SstableWriter(std::string path) {
  fd = open(path.data(), O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    throw std::runtime_error("could not cretae the file for sstable write");
  }
};

void SstableWriter::fill_buffer(std::vector<Memtable::Record> &records,
                                std::vector<std::byte> &buffer) {
  std::size_t curr_pos{HEADER_SIZE};

  for (auto record : records) {
  }
};

void SstableWriter::flush_memtable(Memtable &memtable) {
  std::vector<Memtable::Record> records{memtable.linear_iteration()};

  std::vector<std::byte> buffer(BUFFER_SIZE);
};
