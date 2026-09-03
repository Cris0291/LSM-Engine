#include "LsmEngine.h"
#include "operation.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <random>
#include <string>

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

  std::size_t total_memory{bytes_convertion(memtable.total_byte_count)};

  if (total_memory >= size_threshold) {
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

std::pair<std::string, std::string> create_path() {
  std::filesystem::path dir_path;
  std::filesystem::path file_path;
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
