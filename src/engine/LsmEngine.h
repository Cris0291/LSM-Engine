#pragma once

#include "engine_op.h"
#include "k_merge.h"
#include "memtable.h"
#include "operation.h"
#include "sstable.h"
#include "wal.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>

class LsmEngine {
private:
  static constexpr std::size_t LEVELS{4};
  const std::string LOCK_PATH{"LOCK"};
  const std::string WAL_PATH{"wal.log"};
  const std::string SSTABLE_DIR{"sstable"};
  std::size_t size_threshold;
  std::size_t table_target_size{};
  MemoryUnit memory_unit;
  int fd_flock;
  std::filesystem::path dir;
  std::filesystem::path sstable_dir;
  std::uint32_t sstable_count{0};
  std::vector<std::vector<std::shared_ptr<Sstable>>> records;
  Memtable memtable;
  Wal wal;
  KMerge k_merge{};
  std::size_t bytes_convertion(std::size_t bytes);
  std::string create_path();
  std::uint32_t generate_seed();
  bool surpass_threshold();
  void flush_state();
  int set_flock(std::string flock_path);
  void fsync_dir(std::string dir);
  void compact_records();
  std::shared_ptr<Sstable>
  search_level(const std::vector<std::byte> &key,
               const std::vector<std::shared_ptr<Sstable>> &level);
  void add_to_level(std::size_t level, const std::shared_ptr<Sstable> &&table);
  void remove_from_level(std::size_t level, std::size_t index);
  void sort_level(std::vector<std::shared_ptr<Sstable>> &level);
  std::vector<Record> merge_tables(std::span<std::shared_ptr<Sstable>> tables,
                                   bool is_last_level);
  std::vector<std::shared_ptr<Sstable>>
  split_merge(std::vector<Record> &records);
  std::shared_ptr<Sstable> create_table(std::span<Record> recods);
  std::pair<std::optional<int>, std::optional<int>>
  query_overlap_tables(std::span<std::shared_ptr<Sstable>> tables,
                       std::vector<std::byte> &min_key,
                       std::vector<std::byte> &max_key);
  friend class EngineTest_LevelReadPath_Test;
  friend class EngineTest_MergeWithManyTables_Test;
  friend class EngineTest_MergeNewestWinsWithManyTables_Test;
  friend class EngineTest_TombstoneLast_Test;
  friend class EngineTest_TombstoneNotLast_Test;
  friend class EngineTest_SplitMergeIsEqual_Test;
  friend class EngineTest_SplitMergeIsDisjoint_Test;
  friend class EngineTest_SplitMergeIsOrdered_Test;
  friend class EngineTest_QueryOverlapRangeWithMultipleTables_Test;
  friend class EngineTest_QueryOverlapRangeWithOneTables_Test;
  friend class EngineTest_QueryOverlapRangeNoTablesAfter_Test;
  friend class EngineTest_QueryOverlapRangeNoTablesBefore_Test;
  friend class EngineTest_QueryOverlapRangeNoMiddleTable_Test;
  friend class EngineTest_QueryOverlapRangeAllTable_Test;

public:
  LsmEngine(std::string dir_path, std::size_t threshold, MemoryUnit unit,
            std::size_t _table_target_size = 2000000);
  ~LsmEngine();
  std::optional<std::vector<std::byte>> get(std::vector<std::byte> key);
  void put(std::vector<std::byte> key, std::vector<std::byte> value);
  void delete_record(std::vector<std::byte> key);
};
