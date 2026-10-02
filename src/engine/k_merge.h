#pragma once

#include "sstable.h"
#include <cstddef>
#include <functional>
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
  std::vector<Sstable::Iterator> iteratos;
  std::priority_queue<DataEntry, std::vector<DataEntry>,
                      std::greater<DataEntry>>
      min_heap;

public:
  void flush();
  void seed_k(std::span<std::shared_ptr<Sstable>> tables);
  void merge_k();
};
