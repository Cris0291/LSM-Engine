#include "sstable.h"
#include <fcntl.h>
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
        "soething went wriong while reading the sstable file");
  }

  file_size = file_info.st_size;
};

Memtable::Record Sstable::read(std::vector<std::byte> key) {
  std::size_t footer_offset{file_size - FOOTER_SIZE};
  std::vector<std::byte> footer{};
  std::size_t footer_res{};
  ssize_t read_res{};

  while (footer_res < FOOTER_SIZE) {
    read_res = pread(fd, footer.data(), FOOTER_SIZE, footer_offset);
    footer_res += read_res;
  }
};
