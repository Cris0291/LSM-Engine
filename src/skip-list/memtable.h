#include "node.h"
#include <cstring>
#include <utility>

class Memtable {
private:
  static constexpr int MAX_HEIGHT{32};
  int current_height{0};
  Node *top;
  int compare_bytes(const std::vector<std::byte> &a,
                    const std::vector<std::byte> &b);
  std::pair<Node *, bool> search_for_node(const std::vector<std::byte> &key,
                                          const std::vector<std::byte> &value);

public:
  Node *search(std::vector<std::byte> key, std::vector<std::byte> value);
  void insert(std::vector<std::byte> key, std::vector<std::byte> value);
};
