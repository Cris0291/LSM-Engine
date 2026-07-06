#include "record-encoder.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

class Wal {
private:
  // I need to establish a path for a temp file to dump the bytes
  static constexpr std::size_t BUFFER_SIZE{4096};
  const char *directory_path;
  const char *file_path;
  int fd;
  std::size_t get_size();

public:
  Wal(const char *d_path, const char *f_path);
  ~Wal();
  void append(OperationRecord op, std::span<std::byte> key,
              std::span<std::byte> value);
  std::vector<Record> replay();
  std::vector<Record> replay_whole_file();
  void reset();
};
