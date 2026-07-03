#include "wal.h"
#include <sys/types.h>

Wal::Wal(const char *d_path, const char *f_path)
    : directory_path(d_path), file_path(f_path) {
  fd = open(file_path, O_RDWR | O_CREAT, 0644);
  if (fd == -1)
    throw std::runtime_error("file could not be opened");

  int dir_fd{open(directory_path, O_DIRECTORY)};
  if (dir_fd == -1)
    throw std::runtime_error("directory could not be found");

  if (fsync(dir_fd) == -1)
    throw std::runtime_error("error calling fsync");

  close(dir_fd);
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
  size_t total_size{encode_result.size()};
  size_t written{};
  while (written < total_size) {
    ssize_t bytes_written = write(fd, encode_result.data() + written,
                                  encode_result.size() - written);

    if (bytes_written == -1) {
      throw std::length_error(
          "this is a temporary simple first veriosn of the class");
    }

    written += bytes_written;
    file_size += bytes_written;
  }

  // 3. force to flush with fsync i dont know if i should look a better strategy
  // for handling errors
  if (fsync(fd) == -1) {
    throw std::runtime_error("error calling fsync");
  }
}

bool Wal::replay() {
  std::vector<std::byte> buffer(BUFFER_SIZE);
  std::size_t bytes_read{};
  std::size_t chunk_bytes{};
  // i guess this will point to GOOD as default
  DecodeResult decode_result{};
  std::size_t pending_bytes{};
  ssize_t bytes{};
  bool is_pending{};

  while (bytes_read < file_size) {
    if (decode_result.status == DecodeStatus::TRUNCATED) {
      // for now lets use memmove later will change to a ring buffer
      pending_bytes = buffer.size() - decode_result.bytes_read;
      std::memmove(buffer.data(), buffer.data() + decode_result.bytes_read,
                   pending_bytes);
      is_pending = true;
    }
    if (pending_bytes) {
      bytes =
          read(fd, buffer.data() + pending_bytes, BUFFER_SIZE - pending_bytes);
    } else {
      bytes = read(fd, buffer.data(), BUFFER_SIZE);
    }

    if (bytes_read < 0)
      throw std::runtime_error("chunk could not be read");

    if (bytes_read == 0) {
      if (is_pending)
        throw std::runtime_error("tail torn");

      return true;
    }

    chunk_bytes = 0;
    while (chunk_bytes < bytes) {
      decode_result =
          RecordEncoder::decode(std::span(buffer).subspan(chunk_bytes));
      if (decode_result.status == DecodeStatus::GOOD) {
        bytes_read += decode_result.bytes_read;
        chunk_bytes += chunk_bytes;
        // this result must be used to build the memtable
        if (is_pending)
          is_pending = false;
        continue;
      }
      if (decode_result.status == DecodeStatus::CORRUPTED) {
        throw std::runtime_error("some record was corrupted");
      }

      // this is the ugly path right now we must deal with a potential tail torn
      // or fake trunctation
      if (decode_result.status == DecodeStatus::TRUNCATED && !is_pending) {
        // in this case we have to fetch another chunk in orhter to see if there
        // is a problem
        break;
      }
      // previously we had a truncation
      throw std::runtime_error("record was either truncated or corrupted");
    }
  }
  return true;
}
