#include "node.h"
#include <algorithm>

class Memtable {
private:
  static constexpr int MAX_HEIGHT{32};
  int current_height{0};
  Node *top;
  bool compare_bytes(const std::vector<std::byte> &a,
                     const std::vector<std::byte> &b);

public:
  Node *search(std::vector<std::byte> key, std::vector<std::byte> value);
};
