#include "memtable.h"
#include <algorithm>
#include <bit>
#include <cstdint>

Memtable::Memtable(uint32_t _seed) : seed(_seed), rng(seed) {
  Node *node{new Node(MAX_HEIGHT)};
  top = node;
};

std::pair<Node *, bool>
Memtable::search_for_node(const std::vector<std::byte> &key,
                          std::vector<Node *> &update) {
  Node *temp{top};
  Node *res{nullptr};
  int temp_level{current_height};
  int comparison_res{};

  while (temp_level > 0) {
    if (!temp->forward_list[temp_level]) {
      if (update.back() != temp)
        update.push_back(temp);
      temp_level -= 1;
      continue;
    }
    comparison_res = compare_bytes(key, temp->forward_list[temp_level]->key);
    // Horizontal path if value is less
    if (comparison_res < 0) {
      temp = temp->forward_list[temp_level];
      continue;
    } else if (comparison_res > 0) {
      // Vertical path if value is greater
      if (update.back() != temp)
        update.push_back(temp);
      temp_level -= 1;
      continue;
    } else {
      res = temp->forward_list[temp_level];
      break;
    }
  }

  if (res)
    return {res, true};

  // lineal search over the level 0
  while (temp->forward_list[temp_level]) {
    comparison_res = compare_bytes(key, temp->forward_list[temp_level]->key);
    if (comparison_res < 0) {
      temp = temp->forward_list[temp_level];
      continue;
    } else if (comparison_res > 0) {
      break;
    } else {
      return {temp, true};
    }
  }

  return {temp, false};
};

Node *Memtable::search(std::vector<std::byte> key) {
  std::vector<Node *> update{};
  auto res{search_for_node(key, update)};

  if (res.second)
    return res.first;

  return nullptr;
};

void Memtable::insert(std::vector<std::byte> key, std::vector<std::byte> value,
                      OperationRecord op) {
  std::vector<Node *> update{};
  auto res{search_for_node(key, update)};
  Node *temp{nullptr};

  // first case the node was already there we just update
  if (res.second) {
    res.first->value = std::move(value);
    return;
  }

  // second case we need to insert

  int height{random_height()};
  Node *node{new Node(std::move(key), std::move(value), op, height)};
  Node *after{res.first->forward_list[0]};
  res.first->forward_list[0] = node;
  node->forward_list[0] = after;

  if (height == 1)
    return;

  auto it{update.rbegin()};

  int curr_level{1};
  for (; it != update.rend(); it++) {
    if (curr_level == height)
      break;

    temp = *it;
    after = temp->forward_list[curr_level];
    temp->forward_list[curr_level] = res.first;
    res.first->forward_list[curr_level] = after;
    curr_level += 1;
  }

  if ((height - curr_level) == 0)
    return;

  while (curr_level < height) {
    after = top->forward_list[current_height];
    top->forward_list[current_height] = res.first;
    res.first->forward_list[current_height] = after;
    current_height += 1;
  }

  return;
};

bool Memtable::delete_node(std::vector<std::byte> key) {
  std::vector<Node *> update{};
  auto res{search_for_node(key, update)};

  if (res.second) {
    res.first->tombstone = true;
  }

  return res.second;
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
