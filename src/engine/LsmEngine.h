#pragma once

#include "engine_op.h"
#include "memtable.h"
#include "sstable.h"
#include "wal.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>

class LsmEngine {
private:
  static constexpr std::string LOCK_PATH{"/LOCK"};
  static constexpr std::string WAL_PATH{"wal.log"};
  std::size_t size_threshold;
  MemoryUnit memory_unit;
  int fd_flock;
  std::vector<std::unique_ptr<Sstable>> records;
  Memtable memtable;
  Wal wal;
  std::size_t bytes_convertion(std::size_t bytes);
  std::pair<std::string, std::string> create_path();
  std::string generate_unique_number_id();
  std::uint32_t generate_seed();
  bool surpass_threshold();
  void flush_state();
  int set_flock(std::string flock_path);

public:
  LsmEngine(std::string dir_path, std::size_t threshold, MemoryUnit unit);
  std::optional<std::vector<std::byte>> get(std::vector<std::byte> key);
  void put(std::vector<std::byte> key, std::vector<std::byte> value);
  void delete_record(std::vector<std::byte> key);
};
