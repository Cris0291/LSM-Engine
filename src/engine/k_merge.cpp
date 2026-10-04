#include "k_merge.h"
#include "operation.h"
#include "sstable.h"

void KMerge::seed_k(std::span<std::shared_ptr<Sstable>> tables) {
  for (auto table : tables) {
    iteratos.push_back(table->begin());
  }

  for (std::size_t i{}; i < iteratos.size(); ++i) {
    min_heap.push({(*iteratos[i]).key, i});
  }
}

std::vector<Record> KMerge::merge_k(bool is_last_level) {
  std::vector<Record> res{};

  while (!min_heap.empty()) {
    const DataEntry curr_entry{std::move(min_heap.top())};
    min_heap.pop();

    while (true) {
      const DataEntry &next_entry{min_heap.top()};
      int equal_res{compare_bytes(curr_entry.key, next_entry.key)};
      if (equal_res != 0)
        break;
      auto &it{iteratos[next_entry.rank]};
      min_heap.pop();
      if (it.valid()) {
        ++it;
        DataEntry new_entry{(*it).key, next_entry.rank};
        min_heap.push(std::move(new_entry));
      }
    }

    auto &curr_it{iteratos[curr_entry.rank]};
    if (!is_last_level || (*curr_it).op != OperationRecord::DELETE) {
      res.push_back({(*curr_it).key, (*curr_it).value, (*curr_it).op});
    }
    if (curr_it.valid()) {
      ++curr_it;
      DataEntry new_entry{(*curr_it).key, curr_entry.rank};
      min_heap.push(std::move(new_entry));
    }
  }
  return res;
}

void KMerge::flush() { iteratos.clear(); }
