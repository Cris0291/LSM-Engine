#include "node.h"

Node::Node(std::vector<std::byte> &&_key, std::vector<std::byte> &&_value,
           OperationRecord _op, std::size_t height)
    : key(std::move(_key)), value(std::move(_value)), op(_op),
      forward_list(height) {}
