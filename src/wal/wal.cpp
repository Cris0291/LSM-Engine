#include "wal.h"

Wal::Wal() {
  fd = open(filepath, O_RDWR | O_CREAT, 0644);
  if (fd == -1) {
    throw std::runtime_error("file could not be opened");
  }
}

Wal::~Wal() { close(fd); }

void Wal::append(OperationRecord op, std::span<std::byte> key,
                 std::span<std::byte> value) {
  // for now i am going to hard code a simple version
  // when the engine would be concurrent i would enable
  // group commits and different ways to do the append

  // 1. encode: in this case i dont know if using directly the static class
  // is good for testability or should i hide it on i interface also the class
  // could grow having non static fileds and methods
  std::vector<std::byte> encode_result{RecordEncoder::encode(op, key, value)};

  // 2.write to the file
  ssize_t bytes_written{write(fd, encode_result.data(), encode_result.size())};
  // Should i assert that the bytes written are the same that the ones on the
  // vector or should i leave that to replay
  if (bytes_written == -1) {
    throw std::length_error(
        "this is a temporary simple first veriosn of the class");
  }

  // 3. force to flush with fsync i dont know if i should look a better strategy
  // for handling errors
  if (fsync(fd) == -1) {
    throw std::runtime_error("Error calling fsync");
  }
}
