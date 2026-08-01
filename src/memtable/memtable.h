#pragma once

#include "node.h"
#include <cstdint>
#include <cstring>
#include <random>

class Memtable {
private:
  static constexpr int MAX_HEIGHT{32};
  const uint32_t seed;
  int current_height{0};
  Node *top;
  std::mt19937 rng;
  int total_node_count{};
  Node *search_for_node(const std::vector<std::byte> &key,
                        std::vector<Node *> &update);
  int random_height();
  static std::string to_str(std::vector<std::byte> bytes);

public:
  struct Record {
    std::vector<std::byte> key;
    std::vector<std::byte> value;
    OperationRecord op;
  };
  ~Memtable();
  Memtable(uint32_t _seed);
  Node *search(std::vector<std::byte> key);
  void insert(std::vector<std::byte> key, std::vector<std::byte> value,
              OperationRecord op, bool tombstone);
  void delete_node(std::vector<std::byte> key);
  std::vector<Record> linear_iteration();
};
