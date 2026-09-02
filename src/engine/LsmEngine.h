#pragma once

#include "engine_op.h"
#include "memtable.h"
#include "sstable.h"
#include "wal.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

class LsmEngine {
private:
  std::size_t memory_threshold;
  MemoryUnit memory_unit;
  std::vector<std::unique_ptr<Sstable>> records;
  Memtable memtable;
  Wal wal;

public:
  std::optional<std::vector<std::byte>> get(std::vector<std::byte> key);
  void put(std::vector<std::byte> key, std::vector<std::byte> value);
  void delete_record(std::vector<std::byte> key);
};
