#include "record-encoder.h"
#include <fcntl.h>
#include <unistd.h>

class Wal {
private:
  // I need to establish a path for a temp file to dump the bytes
  static constexpr std::size_t BUFFER_SIZE{4096};
  std::size_t file_size;
  const char *directory_path;
  const char *file_path;
  int fd;
  int file_pos;

public:
  Wal(const char *d_path, const char *f_path);
  ~Wal();
  void append(OperationRecord op, std::span<std::byte> key,
              std::span<std::byte> value);
  void replay();
  void reset();
};
