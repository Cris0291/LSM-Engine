#pragma once

#include "Key.h"
#include "KeyValueRecord.h"
#include "memtable.h"
#include "sstable.h"
#include "wal.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace lsm {

class LsmEngine {
private:
  std::vector<std::unique_ptr<Sstable>> records;
  Memtable memtable;
  Wal wal;
  KeyBytes encode_key(std::string key);
  KeyBytes encode_key(std::uint64_t key);
  KeyBytes encode_key(std::int64_t key);
  KeyBytes encode_key(std::uint32_t key);
  KeyBytes encode_key(std::int32_t key);

public:
  template <Key K, typename V> std::optional<KeyValue<K, V>> get(K key) {
    KeyBytes key_bytes{encode_key(key)};
    memtable.search(key_bytes);
  };
  template <typename K, typename V>
  void put(K key, V value) {

  };
  template <typename K>
  void delete_record(K key) {

  };
};

} // namespace lsm
