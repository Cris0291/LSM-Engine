#include "operation.h"
#include <algorithm>
class Node {
public:
  std::vector<std::byte> key;
  std::vector<std::byte> value;
  OperationRecord op;
  std::vector<Node *> forward_list;
  Node *next;
  Node(std::vector<std::byte> &&_key, std::vector<std::byte> &&_value,
       OperationRecord _op, std::size_t height);
};
