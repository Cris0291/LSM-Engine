#include "LsmEngine.h"
#include "engine_op.h"
#include "memtable.h"
#include "operation.h"
#include "sstable.h"
#include "sstable_writer.h"
#include "wal.h"
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <sys/file.h>
#include <unistd.h>

LsmEngine::LsmEngine(std::string dir_path, std::size_t threshold,
                     MemoryUnit unit)
    : memtable(generate_seed()), size_threshold(threshold), memory_unit(unit) {
  bool dir_exists{std::filesystem::exists(dir_path)};
  bool is_dir{std::filesystem::is_directory(dir_path)};

  if (dir_exists && !is_dir) {
    throw std::runtime_error("path is not a directory");
  }

  if (!dir_exists) {
    bool is_created{std::filesystem::create_directory(dir_path)};
    if (!is_created) {
      throw std::runtime_error("directory could not be created");
    }
  }

  dir = dir_path;

  std::string flock_path{dir_path + LOCK_PATH};
  fd_flock = set_flock(flock_path);

  std::string wal_path{dir_path + WAL_PATH};
  Wal _wal(dir_path.data(), wal_path.data());
  wal = std::move(_wal);

  if (wal.get_size() > 0) {
    std::vector<Record> records{wal.replay()};
    for (Record record : records) {
      memtable.insert(record.key, record.value, record.op,
                      record.op == OperationRecord::DELETE);
    }
  }

  std::string _sstable_dir{dir_path + SSTABLE_DIR};
  if (!std::filesystem::exists(_sstable_dir)) {
    bool is_sstable_created{std::filesystem::create_directory(_sstable_dir)};
    if (!is_sstable_created) {
      throw std::runtime_error("sstble directory could not be created");
    }
  }

  sstable_dir = _sstable_dir;
}

std::optional<std::vector<std::byte>>
LsmEngine::get(std::vector<std::byte> key) {
  std::optional<Memtable::Record> memtabel_res{memtable.search(key)};
  if (memtabel_res.has_value()) {
    return memtabel_res->value;
  }

  for (auto it{records.rbegin()}; it != records.rend(); it++) {
    std::optional<RecordSstable> sstable_res{it->get()->read(key)};
    if (sstable_res.has_value()) {
      return sstable_res->value;
    }
  }

  return {};
}

void LsmEngine::put(std::vector<std::byte> key, std::vector<std::byte> value) {
  wal.append(OperationRecord::PUT, key, value);
  memtable.insert(key, value, OperationRecord::PUT, false);

  if (surpass_threshold()) {
    flush_state();
  }
}

void LsmEngine::delete_record(std::vector<std::byte> key) {
  memtable.delete_node(key);
  std::vector<std::byte> value{};
  wal.append(OperationRecord::DELETE, key, value);

  if (surpass_threshold()) {
    flush_state();
  }
}

std::size_t LsmEngine::bytes_convertion(std::size_t bytes) {
  switch (memory_unit) {
  case MemoryUnit::B: {
    return bytes;
  }
  case MemoryUnit::MB: {
    std::size_t convertion_data{1024 * 1024};
    return bytes / convertion_data;
  }
  case MemoryUnit::KB: {
    return bytes / 1024;
  }
  }
}

std::pair<std::string, std::string> LsmEngine::create_path() {
  std::filesystem::path dir_path;
  std::filesystem::path file_path;

  dir_path = std::filesystem::temp_directory_path() /
             ("sstable_" + generate_unique_number_id());
  std::filesystem::create_directory(dir_path);
  file_path = dir_path / "sstable.txt";

  return {file_path, dir_path};
}

std::string LsmEngine::generate_unique_number_id() {
  auto now{std::chrono::high_resolution_clock::now()};
  auto nanos{std::chrono::duration_cast<std::chrono::nanoseconds>(
                 now.time_since_epoch())
                 .count()};

  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<std::uint64_t> dis;
  auto random_num{dis(gen)};

  return std::to_string(nanos) + "_" + std::to_string(random_num);
}

std::uint32_t LsmEngine::generate_seed() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<std::uint32_t> dis;
  auto random_num{dis(gen)};

  return random_num;
}

bool LsmEngine::surpass_threshold() {
  std::size_t total_memory{bytes_convertion(memtable.total_byte_count)};
  return total_memory >= size_threshold;
}

void LsmEngine::flush_state() {
  auto path{create_path()};
  SstableWriter stable_writer{SstableWriter(path.first, path.second)};
  stable_writer.flush_memtable(memtable);
  records.push_back(std::make_unique<Sstable>(path.first));

  Memtable new_memtable{Memtable(generate_seed())};
  std::destroy_at(&memtable);
  std::construct_at(&memtable, std::move(new_memtable));
  wal.reset();
}

int LsmEngine::set_flock(std::string flock_path) {
  int _fd_flock{open(flock_path.data(), O_RDWR | O_CREAT, 0666)};
  if (_fd_flock == -1) {
    throw std::runtime_error(
        "lock file does not exist, directory does not belong to an engine");
  }
  int lock_res{flock(_fd_flock, LOCK_EX | LOCK_NB)};
  if (lock_res == -1) {
    int error;
    close(_fd_flock);
    error = errno;
    if (error == EWOULDBLOCK) {
      throw std::runtime_error("flock was already in use");
    }

    throw std::runtime_error("there was a problem locking the flock file");
  }
  return _fd_flock;
}
