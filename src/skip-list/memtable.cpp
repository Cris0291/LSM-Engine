#include "memtable.h"
#include <algorithm>
#include <bit>
#include <cstdint>

Memtable::Memtable(uint32_t _seed) : seed(_seed), rng(seed){};

bool Memtable::search_for_node(const std::vector<std::byte> &key,
                               std::vector<Node *> &update) {
  // in this case update is needed to record the path needed to reach the 0 list
  // in update just the nodes that changed level the ones that went downward are
  // being pushed since this si the rightmost view and the one that actually is
  // relevant to the node once it is promoted the last node of update has two
  // menings either it is the node we are looking for or an approximation of the
  // last level

  Node *temp{top};
  Node *res{nullptr};
  int temp_level{current_height};
  int comparison_res{};

  while (temp_level > 0) {
    if (!temp->next) {
      temp = temp->forward_list[temp_level--];
      update.push_back(temp);
      continue;
    }
    comparison_res = compare_bytes(key, temp->next->key);
    // Horizontal path if value is less
    if (comparison_res < 0) {
      temp = temp->next;
      continue;
    } else if (comparison_res > 0) {
      // Vertical path if value is greater
      temp = temp->forward_list[temp_level--];
      update.push_back(temp);
      continue;
    } else {
      // there should be no duplication but i dont know how this would work with
      // bit patterns for now lets assume that no duplication might happen i
      // will refine later if key is not the same for now i will throw
      res = temp->next;
      update.push_back(res);
      break;
    }
  }

  if (res)
    return true;

  return false;
};

Node *Memtable::search(std::vector<std::byte> key) {
  Node *res{nullptr};
  int comparison_res{};
  std::vector<Node *> update{};
  auto is_same_node{search_for_node(key, update)};

  if (is_same_node) {
    return update.back();
  } else {
    // in this case we hit the bottom list without an early find
    // so we need to iterate over the last linkeked list segment until we find
    // our node
    Node *temp{update.back()};
    while (temp->next) {
      comparison_res = compare_bytes(key, temp->next->value);
      if (comparison_res == 0) {
        res = temp->next;
        break;
      }
    }
  }

  return res;
};

void Memtable::insert(std::vector<std::byte> key,
                      std::vector<std::byte> value) {
  std::vector<Node *> update{};
  bool is_same_node{search_for_node(key, update)};
  Node *temp{update.back()};

  // first case the node was already there we just update
  if (is_same_node) {
    temp->value = std::move(value);
    return;
  }

  // second case we need to insert
  while (temp->next) {
    int comparison_res = compare_bytes(key, temp->next->value);
    if (comparison_res < 0) {
      temp = temp->next;
      continue;
    }

    if (comparison_res > 0) {
      // this is the point in which insertion must happen
      break;
    }

    if (comparison_res == 0) {
      temp->value = std::move(value);
      return;
    }
  }
};

int Memtable::compare_bytes(const std::vector<std::byte> &a,
                            const std::vector<std::byte> &b) {
  int res{std::memcmp(a.data(), b.data(), std::min(a.size(), b.size()))};
  return res;
};

int Memtable::random_height() {
  uint32_t random_number{static_cast<uint32_t>(rng())};
  int height{std::min(std::countl_zero(random_number) + 1, MAX_HEIGHT)};
  return height;
};
