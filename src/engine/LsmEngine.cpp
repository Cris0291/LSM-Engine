#include "LsmEngine.h"
#include "engine_op.h"
#include "memtable.h"
#include "operation.h"
#include "sstable.h"
#include "sstable_writer.h"
#include "wal.h"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <format>
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

  if (!std::filesystem::is_directory(_sstable_dir)) {
    throw std::runtime_error("sstble was not a directory");
  }

  sstable_dir = _sstable_dir;

  if (!std::filesystem::is_empty(sstable_dir)) {
    try {
      for (const auto &entry :
           std::filesystem::directory_iterator(sstable_dir)) {
        if (!entry.is_directory()) {
          records.push_back(std::make_unique<Sstable>(entry.path()));
        }
      }
    } catch (const std::filesystem::filesystem_error &e) {
      throw;
    }
  }

  if (records.size() > 1) {
    std::sort(records.begin(), records.end(),
              [](const std::unique_ptr<Sstable> &a,
                 const std::unique_ptr<Sstable> &b) {
                return a.get()->sstable_path < b.get()->sstable_path;
              });

    sstable_count = records.size();
  }
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

std::string LsmEngine::create_path() {
  std::string sstable_file{std::format("sstable_{:06d}", sstable_count)};
  sstable_count += 1;
  return sstable_dir + "/" + sstable_file;
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
  std::string path{create_path()};
  SstableWriter stable_writer{SstableWriter(path, sstable_dir)};
  stable_writer.flush_memtable(memtable);
  records.push_back(std::make_unique<Sstable>(path));

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
