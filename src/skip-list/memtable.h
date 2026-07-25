#include "node.h"
#include <cstdint>
#include <cstring>
#include <random>

class Memtable {
private:
  static constexpr int MAX_HEIGHT{32};
  const Node *sentinel_list[MAX_HEIGHT];
  const uint32_t seed;
  int current_height{31};
  Node *top;
  std::mt19937 rng;
  int compare_bytes(const std::vector<std::byte> &a,
                    const std::vector<std::byte> &b);
  bool search_for_node(const std::vector<std::byte> &key,
                       std::vector<Node *> &update);
  int random_height();

public:
  Memtable(uint32_t _seed);
  Node *search(std::vector<std::byte> key);
  void insert(std::vector<std::byte> key, std::vector<std::byte> value,
              OperationRecord op);
  bool delete_node(std::vector<std::byte> key);
};
