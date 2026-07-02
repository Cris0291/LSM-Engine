#include "record-encoder.h"
#include <fcntl.h>
#include <unistd.h>

class Wal {
private:
  // I need to establish a path for a temp file to dump the bytes
  const char *filepath;
  int fd;

public:
  Wal();
  ~Wal();
  void append(OperationRecord op, std::span<std::byte> key,
              std::span<std::byte> value);
  void replay();
  void reset();
};
