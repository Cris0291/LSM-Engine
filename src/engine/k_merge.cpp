#include "k_merge.h"
#include <algorithm>

void KMerge::seed_k(std::span<std::shared_ptr<Sstable>> tables) {
  for (auto table : tables) {
    iteratos.push_back(std::move(table->begin()));
  }
}
