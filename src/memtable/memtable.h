#pragma once

#include "node.h"
#include "operation.h"
#include <cstdint>
#include <cstring>
#include <optional>
#include <random>

class Memtable {
private:
  static constexpr int MAX_HEIGHT{32};
  static constexpr int OP_SIZE{1};
  uint32_t seed;
  int current_height{0};
  Node *top;
  std::mt19937 rng;
  int total_node_count{};
  Node *search_for_node(const std::vector<std::byte> &key,
                        std::vector<Node *> &update);
  int random_height();
  static std::string to_str(std::vector<std::byte> bytes);
  static Record copy_node_to_record(Node *node);

public:
  std::size_t total_byte_count;
  Memtable();
  Memtable(uint32_t _seed);
  Memtable(Memtable &memtable) = delete;
  Memtable &operator=(Memtable &memtable) = delete;
  Memtable(Memtable &&other) noexcept;
  Memtable &operator=(Memtable &&memtable) noexcept;
  ~Memtable();
  std::optional<Record> search(std::vector<std::byte> key);
  void insert(std::vector<std::byte> key, std::vector<std::byte> value,
              OperationRecord op, bool tombstone);
  void delete_node(std::vector<std::byte> key);
  std::vector<Record> linear_iteration();
};
