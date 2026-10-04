#include "LsmEngine.h"
#include "engine_op.h"
#include "lsm_utilities.h"
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
#include <format>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <sys/file.h>
#include <unistd.h>

LsmEngine::LsmEngine(std::string dir_path, std::size_t threshold,
                     MemoryUnit unit)
    : dir(dir_path), memtable(generate_seed()), size_threshold(threshold),
      memory_unit(unit), records(LEVELS) {
  bool dir_exists{std::filesystem::exists(dir)};
  bool is_dir{std::filesystem::is_directory(dir)};

  if (dir_exists && !is_dir) {
    throw std::runtime_error("path is not a directory");
  }

  if (!dir_exists) {
    bool is_created{std::filesystem::create_directory(dir)};
    if (!is_created) {
      throw std::runtime_error("directory could not be created");
    }
  }

  fsync_dir(dir.string());

  std::filesystem::path flock_path{dir / LOCK_PATH};
  fd_flock = set_flock(flock_path.string());

  std::filesystem::path wal_path{dir / WAL_PATH};
  Wal _wal(dir.string().data(), wal_path.string().data());
  wal = std::move(_wal);

  if (wal.get_size() > 0) {
    std::vector<Record> records{wal.replay()};
    for (Record record : records) {
      memtable.insert(record.key, record.value, record.op,
                      record.op == OperationRecord::DELETE);
    }
  }

  std::filesystem::path _sstable_dir{dir / SSTABLE_DIR};
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
          // for now push everything to level 0
          records[0].push_back(std::make_shared<Sstable>(entry.path()));
        }
      }
    } catch (const std::filesystem::filesystem_error &e) {
      throw;
    }
  }

  sstable_count = records.size();

  if (records.size() > 1) {
    std::sort(records[0].begin(), records[0].end(),
              [](const std::shared_ptr<Sstable> &a,
                 const std::shared_ptr<Sstable> &b) {
                return a.get()->sstable_path < b.get()->sstable_path;
              });
  }

  fsync_dir(sstable_dir);
}

LsmEngine::~LsmEngine() { close(fd_flock); }

std::optional<std::vector<std::byte>>
LsmEngine::get(std::vector<std::byte> key) {
  std::optional<Record> memtabel_res{memtable.search(key)};
  if (memtabel_res.has_value()) {
    if (memtabel_res.value().op == OperationRecord::DELETE) {
      return {};
    }
    return memtabel_res.value().value;
  }

  for (auto it{records[0].rbegin()}; it != records[0].rend(); it++) {
    std::optional<Record> sstable_res{it->get()->read(key)};
    if (sstable_res.has_value()) {
      if (sstable_res.value().op == OperationRecord::DELETE) {
        return {};
      }
      return sstable_res.value().value;
    }
  }

  for (int i{1}; i < records.size(); i++) {
    if (records[i].empty())
      continue;
    std::shared_ptr<Sstable> res{search_level(key, records[i])};
    if (res) {
      std::optional<Record> sstable_res{res.get()->read(key)};
      if (sstable_res.has_value()) {
        if (sstable_res.value().op == OperationRecord::DELETE) {
          return {};
        }

        return sstable_res.value().value;
      }
    }
  }

  return {};
}

void LsmEngine::put(std::vector<std::byte> key, std::vector<std::byte> value) {
  wal.append(OperationRecord::PUT, key, value);
  memtable.insert(key, value, OperationRecord::PUT, false);
  std::optional<Record> res{memtable.search(key)};

  if (surpass_threshold()) {
    flush_state();
  }
}

void LsmEngine::delete_record(std::vector<std::byte> key) {
  std::vector<std::byte> value{};
  wal.append(OperationRecord::DELETE, key, value);
  memtable.delete_node(key);

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
  std::filesystem::path sstable_path{sstable_dir / sstable_file};
  return sstable_path.string();
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
  SstableWriter stable_writer{SstableWriter(path)};
  stable_writer.flush_memtable(memtable);
  // this will change later for now min max keys will be handled like this
  auto sstable{std::make_shared<Sstable>(path)};
  sstable.get()->set_min_max(stable_writer.min_key, stable_writer.max_key);
  records[0].push_back(std::move(sstable));

  Memtable new_memtable{Memtable(generate_seed())};
  memtable = std::move(new_memtable);
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

void LsmEngine::fsync_dir(std::string dir) {
  int dir_fd{open(dir.data(), O_DIRECTORY)};
  if (dir_fd == -1) {
    throw std::runtime_error("directory could not be found");
  }

  if (fsync(dir_fd) == -1) {
    throw std::runtime_error("error calling fsync");
  }

  close(dir_fd);
}

std::shared_ptr<Sstable>
LsmEngine::search_level(const std::vector<std::byte> &key,
                        const std::vector<std::shared_ptr<Sstable>> &level) {
  std::size_t low{0};
  std::size_t high{level.size() - 1};

  while (low <= high) {
    std::size_t mid = low + (high - low) / 2;

    int low_comparison{compare_bytes(level[mid].get()->min_key, key)};
    int high_comparison{compare_bytes(level[mid].get()->max_key, key)};

    std::cerr << "low " << low << "mid " << mid << "high " << high
              << "low comparison " << low_comparison << "high_comparison "
              << high_comparison << "\n";

    if (low == high) {
      if ((low_comparison > 0 && high_comparison < 0) || low_comparison == 0 ||
          high_comparison == 0) {
        return level[low];
      } else {
        return nullptr;
      }
    }

    if (low_comparison < 0 && high_comparison < 0) {
      low = mid + 1;
    } else if (low_comparison > 0 && high_comparison > 0) {
      high = mid - 1;
    } else {
      return level[mid];
    }
  }
  return nullptr;
}

void LsmEngine::add_to_level(std::size_t level,
                             const std::shared_ptr<Sstable> &&table) {
  records[level].push_back(std::move(table));
  if (records[level].size() > 1) {
    sort_level(records[level]);
  }
}

void LsmEngine::sort_level(std::vector<std::shared_ptr<Sstable>> &level) {
  std::sort(
      level.begin(), level.end(),
      [](const std::shared_ptr<Sstable> &a, const std::shared_ptr<Sstable> &b) {
        int max_to_min{compare_bytes(a->max_key, b->min_key)};
        return max_to_min < 0;
      });
}

std::vector<Record>
LsmEngine::merge_tables(std::span<std::shared_ptr<Sstable>> tables,
                        bool is_last_level) {
  k_merge.seed_k(tables);
  std::vector<Record> records{k_merge.merge_k(is_last_level)};
  k_merge.flush();
  return records;
}
