#include "node.h"
#include <cstring>

class Memtable {
private:
  static constexpr int MAX_HEIGHT{32};
  int current_height{0};
  Node *top;
  int compare_bytes(const std::vector<std::byte> &a,
                    const std::vector<std::byte> &b);
  bool search_for_node(const std::vector<std::byte> &key,
                       std::vector<Node *> &update);

public:
  Node *search(std::vector<std::byte> key);
  void insert(std::vector<std::byte> key, std::vector<std::byte> value);
};
