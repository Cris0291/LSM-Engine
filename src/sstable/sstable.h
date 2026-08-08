#pragma once

#include "memtable.h"
#include <cstddef>
#include <string>
#include <vector>

class Sstable {
private:
  static constexpr std::size_t FOOTER_SIZE{20};
  int fd;
  std::size_t file_size;

public:
  Sstable(std::string path);
  Memtable::Record read(std::vector<std::byte> key);
};
