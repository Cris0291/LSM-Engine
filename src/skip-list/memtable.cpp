#include "memtable.h"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <optional>

Memtable::Memtable(uint32_t _seed) : seed(_seed), rng(seed) {
  Node *node{new Node(MAX_HEIGHT)};
  top = node;
};

Node *Memtable::search_for_node(const std::vector<std::byte> &key,
                                std::vector<Node *> &update) {
  Node *temp{top};
  Node *res{nullptr};
  int temp_level{current_height};
  int comparison_res{};

  while (temp_level > 0) {
    if (!temp->forward_list[temp_level]) {
      update[temp_level] = temp;
      temp_level -= 1;
      continue;
    }
    comparison_res = compare_bytes(temp->forward_list[temp_level]->key, key);
    // Horizontal path if value is less
    if (comparison_res < 0) {
      temp = temp->forward_list[temp_level];
      continue;
    } else if (comparison_res > 0) {
      // Vertical path if value is greater
      update[temp_level] = temp;
      temp_level -= 1;
      continue;
    } else {
      res = temp->forward_list[temp_level];
      break;
    }
  }

  if (res)
    return res;

  // lineal search over the level 0
  while (temp->forward_list[temp_level]) {
    comparison_res = compare_bytes(key, temp->forward_list[temp_level]->key);
    if (comparison_res < 0) {
      temp = temp->forward_list[temp_level];
      continue;
    } else if (comparison_res > 0) {
      update[temp_level] = temp;
      break;
    } else {
      return temp->forward_list[temp_level];
    }
  }

  return res;
};

Node *Memtable::search(std::vector<std::byte> key) {
  std::vector<Node *> update(MAX_HEIGHT, top);
  Node *res{search_for_node(key, update)};

  if (res)
    return res;

  return nullptr;
};

void Memtable::insert(std::vector<std::byte> key, std::vector<std::byte> value,
                      OperationRecord op, bool tombstone) {
  std::vector<Node *> update(MAX_HEIGHT, top);
  Node *res{search_for_node(key, update)};
  Node *temp{nullptr};
  Node *after{nullptr};
  int curr_level{0};

  // first case the node was already there we just update
  if (res && !tombstone) {
    res->value = std::move(value);
    return;

  } else if (res && tombstone) {
    res->value = value;
    res->op = op;
    return;
  }

  // second case we need to insert

  int height{random_height()};
  Node *node{new Node(std::move(key), std::move(value), op, height)};

  for (int i{}; i < update.size(); i++) {
    if (curr_level == height)
      break;

    temp = update[i];
    after = temp->forward_list[curr_level];
    temp->forward_list[curr_level] = node;
    node->forward_list[curr_level] = after;
    curr_level += 1;
  }

  if (height > current_height) {
    int to_be_added_levels{height - current_height};
    current_height += to_be_added_levels;
  }
  return;
};

void Memtable::delete_node(std::vector<std::byte> key) {
  insert(key, {}, OperationRecord::DELETE, true);
  return;
};

int Memtable::compare_bytes(const std::vector<std::byte> &a,
                            const std::vector<std::byte> &b) {
  int res{std::memcmp(a.data(), b.data(), std::min(a.size(), b.size()))};
  if (res == 0) {
    if (a.size() < b.size()) {
      res = -1;
    } else if (a.size() > b.size()) {
      res = 1;
    } else {
      res = 0;
    }
  }
  return res;
};

int Memtable::random_height() {
  uint32_t random_number{static_cast<uint32_t>(rng())};
  int height{std::min(std::countl_zero(random_number) + 1, MAX_HEIGHT)};
  return height;
};

Memtable::~Memtable() {
  Node *temp{nullptr};
  while (top) {
    temp = top->forward_list[0];
    delete top;
    top = temp;
  }
};
