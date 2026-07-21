#include "memtable.h"
#include <algorithm>
#include <stdexcept>

std::pair<Node *, bool>
Memtable::search_for_node(const std::vector<std::byte> &key,
                          const std::vector<std::byte> &value) {
  Node *temp{top};
  Node *res{nullptr};
  int temp_level{current_height};
  int comparison_res{};

  while (temp_level > 0) {
    if (!temp->next) {
      temp = temp->forward_list[temp_level--];
      continue;
    }
    comparison_res = compare_bytes(value, temp->next->value);
    // Horizontal path if value is less
    if (comparison_res < 0) {
      temp = temp->next;
      continue;
    } else if (comparison_res > 0) {
      // Vertical path if value is greater
      temp = temp->forward_list[temp_level--];
      continue;
    } else {
      // there should be no duplication but i dont know how this would work with
      // bit patterns for now lets assume that no duplication might happen i
      // will refine later if key is not the same for now i will throw
      int compare_key = compare_bytes(key, temp->next->key);
      if (compare_key != 0)
        throw std::runtime_error("duplicate node found");
      res = temp->next;
      break;
    }
  }

  if (res)
    return {res, true};

  return {temp, false};
};

Node *Memtable::search(std::vector<std::byte> key,
                       std::vector<std::byte> value) {
  Node *res{nullptr};
  int comparison_res{};
  auto res_search{search_for_node(key, value)};

  if (res_search.second) {
    return res_search.first;
  } else {
    // in this case we hit the bottom list without an early find
    // so we need to iterate over the last linkeked list segment until we find
    // our node
    Node *temp{res_search.first};
    while (temp->next) {
      comparison_res = compare_bytes(value, temp->next->value);
      if (comparison_res == 0) {
        int compare_key = compare_bytes(key, temp->next->key);
        if (compare_key != 0)
          throw std::runtime_error("duplicate node found");
        res = temp->next;
        break;
      }
    }
  }

  return res;
};

void Memtable::insert(std::vector<std::byte> key,
                      std::vector<std::byte> value) {

};

int Memtable::compare_bytes(const std::vector<std::byte> &a,
                            const std::vector<std::byte> &b) {
  int res{std::memcmp(a.data(), b.data(), std::min(a.size(), b.size()))};
  return res;
}
