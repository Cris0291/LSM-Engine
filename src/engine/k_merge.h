#pragma once

#include "lsm_utilities.h"
#include "operation.h"
#include "sstable.h"
#include <cstddef>
#include <memory>
#include <queue>
#include <span>
#include <vector>

class KMerge {
private:
  struct DataEntry {
    std::vector<std::byte> key;
    std::size_t rank;
  };
  struct Comparator {
    bool operator()(const DataEntry &a, const DataEntry &b) {
      int res{compare_bytes(a.key, b.key)};
      return res > 0;
    }
  };
  std::vector<Sstable::Iterator> iteratos{};
  std::priority_queue<DataEntry, std::vector<DataEntry>, Comparator> min_heap{};

public:
  void flush();
  void seed_k(std::span<std::shared_ptr<Sstable>> tables);
  std::vector<Record> merge_k(bool is_last_level);
};
