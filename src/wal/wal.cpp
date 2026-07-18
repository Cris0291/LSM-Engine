#include "wal.h"
#include <cstddef>
#include <iostream>
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
    ssize_t bytes_written =
        write(fd, encode_result.data() + written, total_size - written);

    if (bytes_written == -1) {
      throw std::length_error(
          "this is a temporary simple first veriosn of the class");
    }

    written += bytes_written;
  }

  // 3. force to flush with fsync i dont know if i should look a better strategy
  // for handling errors
  if (fsync(fd) == -1) {
    throw std::runtime_error("error calling fsync");
  }
}

std::vector<Record> Wal::replay() {
  std::size_t file_size{get_size()};
  std::size_t bytes_processed{};
  std::vector<std::byte> buffer(BUFFER_SIZE);
  std::size_t current_buffer_size{BUFFER_SIZE};
  std::vector<Record> res{};
  ReplayResult replay_res{};
  ReplayState stateNow{ReplayState::NEWSTATE};
  ReplayState stateNext{ReplayState::NEWSTATE};
  ReplayInternalState internal_state{ReplayInternalState::NOSTATE};
  bool last_decode_guard{false};

  while (bytes_processed < file_size || last_decode_guard) {

    switch (stateNow) {
    case ReplayState::NEWSTATE: {

      if (replay_res.state == ReplayInternalState::TRUNCATED ||
          replay_res.state == ReplayInternalState::CORRUPTED) {
        // Note that these two states are being evaluated together because
        // the decoder can sometimes imply corruption for truncation
        if (replay_res.consumed_bytes == 0) {
          stateNext = ReplayState::RESIZE;
        } else if (replay_res.consumed_bytes != 0) {
          stateNext = ReplayState::MOVE;
        }
      } else if (replay_res.state == ReplayInternalState::PROCESS) {
        stateNext = ReplayState::DECODE;
      } else if (replay_res.state == ReplayInternalState::ENDOFFILE) {
        // this also terminates the fsm
        return res;
      } else if (replay_res.state == ReplayInternalState::BYTESLESSTHANZERO) {
        // This terminates the fsm
        return res;
      } else {
        // this setting to zero is happenign becuase this branch
        // can only ever be reached up by a successful decode
        replay_res.total_buffer_bytes = 0;
        replay_res.pos_offset = 0;
        replay_res.leftover = current_buffer_size;
        stateNext = ReplayState::READ;
      }
      break;
    }
    case ReplayState::DECODE: {

      auto pair = replay_decode(res, buffer, replay_res.total_buffer_bytes);
      replay_res.consumed_bytes = pair.first;
      if (pair.second == DecodeStatus::GOOD) {
        replay_res.state = ReplayInternalState::NOSTATE;
      } else if (pair.second == DecodeStatus::TRUNCATED) {
        replay_res.state = ReplayInternalState::TRUNCATED;
      } else {
        replay_res.state = ReplayInternalState::CORRUPTED;
      }
      replay_res.pos_offset = pair.first;
      replay_res.leftover =
          replay_res.total_buffer_bytes - replay_res.consumed_bytes;
      stateNext = ReplayState::NEWSTATE;
      break;
    }
    case ReplayState::MOVE: {

      std::size_t new_total_buffer_size{replay_res.total_buffer_bytes -
                                        replay_res.consumed_bytes};
      replay_move(buffer, replay_res.consumed_bytes, new_total_buffer_size);
      replay_res.total_buffer_bytes = new_total_buffer_size;
      replay_res.pos_offset = new_total_buffer_size;
      replay_res.leftover = current_buffer_size - replay_res.total_buffer_bytes;
      replay_res.consumed_bytes = 0;
      replay_res.state = ReplayInternalState::NOSTATE;
      stateNext = ReplayState::READ;
      break;
    }
    case ReplayState::RESIZE: {
      std::cout << "7" << "\n";
      current_buffer_size = replay_resize(buffer);
      replay_res.pos_offset = replay_res.total_buffer_bytes;
      replay_res.leftover = current_buffer_size - replay_res.total_buffer_bytes;
      stateNext = ReplayState::READ;
      break;
    }
    case ReplayState::READ: {

      ssize_t read_result = replay_read(buffer, replay_res.pos_offset,
                                        replay_res.leftover, bytes_processed);
      if (read_result < 0) {
        replay_res.state = ReplayInternalState::BYTESLESSTHANZERO;
      } else if (read_result == 0) {
        replay_res.state = ReplayInternalState::ENDOFFILE;
      } else {
        bytes_processed += read_result;
        replay_res.total_buffer_bytes += read_result;
        replay_res.state = ReplayInternalState::PROCESS;
        if (bytes_processed >= file_size && replay_res.total_buffer_bytes > 0) {
          last_decode_guard = true;
        }
      }
      stateNext = ReplayState::NEWSTATE;
      break;
    }
    }

    stateNow = stateNext;
  }

  return res;
}

std::pair<std::size_t, DecodeStatus>
Wal::replay_decode(std::vector<Record> &res, std::vector<std::byte> &buffer,
                   std::size_t buffer_size) {
  std::size_t chunk_bytes = 0;
  DecodeResult decode_result{};
  while (chunk_bytes < buffer_size) {
    decode_result =
        RecordEncoder::decode(std::span(buffer).subspan(chunk_bytes));
    if (decode_result.status == DecodeStatus::GOOD) {
      chunk_bytes += decode_result.bytes_read;
      res.emplace_back(decode_result);
      continue;
    }
    if (decode_result.status == DecodeStatus::CORRUPTED) {
      // it is necessary to distinguish between a truncation or corruption
      // that happened at the tail from one that happen in the middle of the
      // file for now lets just return the decoded records
      break;
    }

    // this is the ugly path right now we must deal with a potential tail torn
    // or fake trunctation
    if (decode_result.status == DecodeStatus::TRUNCATED) {
      // in this case we have to fetch another chunk in orhter to see if there
      // is a problem
      break;
    }
    // previously we had a truncation
    throw std::runtime_error("record was either truncated or corrupted");
  }
  return {chunk_bytes, decode_result.status};
}

ssize_t Wal::replay_read(std::vector<std::byte> &buffer, std::size_t pos_offset,
                         std::size_t leftover, std::size_t file_pos) {

  ssize_t bytes = pread(fd, buffer.data() + pos_offset, leftover, file_pos);

  return bytes;
}

void Wal::replay_move(std::vector<std::byte> &buffer, std::size_t consumed,
                      std::size_t pending) {
  std::memmove(buffer.data(), buffer.data() + consumed, pending);
}

std::size_t Wal::replay_resize(std::vector<std::byte> &buffer) {
  std::size_t old_size = buffer.size();
  buffer.resize(old_size * 2);
  return buffer.size();
}

std::vector<Record> Wal::replay_whole_file() {
  std::size_t file_size{get_size()};
  std::vector<std::byte> buffer(file_size);
  std::vector<Record> res{};
  std::size_t bytes_read{};
  std::size_t to_be_read{};
  DecodeResult decode_result{};

  while (bytes_read < file_size) {
    ssize_t bytes =
        read(fd, buffer.data() + bytes_read, file_size - bytes_read);
    if (bytes == 0)
      break;
    if (bytes < 0)
      throw std::runtime_error("file could not be read");
    bytes_read += bytes;
  }

  while (to_be_read < bytes_read) {
    decode_result =
        RecordEncoder::decode(std::span(buffer).subspan(to_be_read));
    if (decode_result.status == DecodeStatus::GOOD) {
      to_be_read += decode_result.bytes_read;
      res.emplace_back(decode_result);
      continue;
    }
    if (decode_result.status == DecodeStatus::CORRUPTED) {
      // it is necessary to distinguish between a truncation or corruption
      // that happened at the tail from one that happen in the middle of the
      // file for now lets just return the decoded records
      return res;
    }

    // this is the ugly path right now we must deal with a potential tail torn
    // or fake trunctation
    if (decode_result.status == DecodeStatus::TRUNCATED) {
      return res;
    }
  }
  return res;
}

std::size_t Wal::get_size() {
  struct stat file_info;
  fstat(fd, &file_info);
  return file_info.st_size;
}
