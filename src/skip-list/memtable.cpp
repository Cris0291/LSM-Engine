#include "memtable.h"
#include <algorithm>

Node *Memtable::search(std::vector<std::byte> key,
                       std::vector<std::byte> value) {
  Node *temp{top};
  Node *res{nullptr};
  int temp_level{current_height};

  while (temp_level > 0) {
    if (compare_bytes(value, temp->next->value)) {
    }
  }
};

bool Memtable::compare_bytes(const std::vector<std::byte> &a,
                             const std::vector<std::byte> &b) {
  bool is_less{
      std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end())};
  return is_less;
}
