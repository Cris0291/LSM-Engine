#include "lsm_utilities.h"
#include "memtable.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <random>
#include <string>

static std::vector<std::byte> bytes(const std::string &s) {
  std::vector<std::byte> v;
  v.reserve(s.size());
  for (char c : s) {
    v.push_back(static_cast<std::byte>(c));
  }
  return v;
}

TEST(MemtableTest, CompareEqualBytesReflexiveProperty) {
  // Arrange
  std::string value{"TEST"};
  std::vector<std::byte> value_bytes{bytes(value)};

  // Act
  int res{compare_bytes(value_bytes, value_bytes)};

  // Assert
  EXPECT_EQ(res, 0);
};

TEST(MemtableTest, CompareLessAndGreater) {
  // Arrange
  std::string key1{"apple"};
  std::string key2{"banana"};
  std::string key3{"elderberry"};
  std::vector<std::byte> bytes_key1{bytes(key1)};
  std::vector<std::byte> bytes_key2{bytes(key2)};
  std::vector<std::byte> bytes_key3{bytes(key3)};

  // Act
  int res{compare_bytes(bytes_key1, bytes_key2)};
  int res1{compare_bytes(bytes_key3, bytes_key1)};

  // Assert
  EXPECT_TRUE(res < 0);
  EXPECT_TRUE(res1 > 0);
};

TEST(MemtableTest, CompareAntisymmetricProperty) {
  // Arrange
  std::string lex_less{"ab"};
  std::string lex_greater{"az"};
  std::vector<std::byte> bytes_lex_less{bytes(lex_less)};
  std::vector<std::byte> bytes_lex_greater{bytes(lex_greater)};

  // Act
  int res_less{compare_bytes(bytes_lex_less, bytes_lex_greater)};
  int res_greater{compare_bytes(bytes_lex_greater, bytes_lex_less)};

  // Assert
  EXPECT_TRUE(res_less < 0);
  EXPECT_TRUE(res_greater > 0);
};

TEST(MemtableTest, CompareTransitiveProperty) {
  // Arrange
  std::string lex_first{"aa"};
  std::string lex_second{"ab"};
  std::string lex_third{"ad"};
  std::vector<std::byte> bytes_first{bytes(lex_first)};
  std::vector<std::byte> bytes_second{bytes(lex_second)};
  std::vector<std::byte> bytes_third{bytes(lex_third)};

  // Act
  int res_first_second{compare_bytes(bytes_first, bytes_second)};
  int res_second_third{compare_bytes(bytes_second, bytes_third)};
  int res_first_third{compare_bytes(bytes_first, bytes_third)};

  // Assert
  EXPECT_TRUE(res_first_second < 0);
  EXPECT_TRUE(res_second_third < 0);
  EXPECT_TRUE(res_first_third < 0);
};

TEST(MemtableTest, SubstringComparison) {
  // Arrange
  std::string sub{"abb"};
  std::string text{"abbac"};
  std::vector<std::byte> bytes_sub{bytes(sub)};
  std::vector<std::byte> bytes_text{bytes(text)};

  // Act
  int res{compare_bytes(bytes_text, bytes_sub)};

  // Arrange
  EXPECT_TRUE(res > 0);
};

TEST(MemtableTest, CompareEmpty) {
  // Arrange
  std::vector<std::byte> empty{};

  // Act
  int res{compare_bytes(empty, empty)};

  // Assert
  EXPECT_TRUE(res == 0);
};

TEST(MemtableTest, MemtableSortedOrder) {
  // Arrange
  std::uint32_t seed{1042};
  Memtable memtable{seed};
  std::string key1{"apple"};
  std::string key2{"banana"};
  std::string key3{"cherry"};
  std::string key4{"date"};
  std::string key5{"elderberry"};
  std::string key6{"1"};
  std::string key7{"2"};

  auto bytes_key1{bytes(key1)};
  auto bytes_key2{bytes(key2)};
  auto bytes_key3{bytes(key3)};
  auto bytes_key4{bytes(key4)};
  auto bytes_key5{bytes(key5)};
  auto bytes_key6{bytes(key6)};
  auto bytes_key7{bytes(key7)};

  // Act
  memtable.insert(bytes_key1, bytes_key1, OperationRecord::PUT, false);
  memtable.insert(bytes_key2, bytes_key2, OperationRecord::PUT, false);
  memtable.insert(bytes_key3, bytes_key3, OperationRecord::PUT, false);
  memtable.insert(bytes_key4, bytes_key4, OperationRecord::PUT, false);
  memtable.insert(bytes_key5, bytes_key5, OperationRecord::PUT, false);
  memtable.insert(bytes_key6, bytes_key6, OperationRecord::PUT, false);
  memtable.insert(bytes_key7, bytes_key7, OperationRecord::PUT, false);

  auto res{memtable.linear_iteration()};

  // Assert
  EXPECT_EQ(res[0].key, bytes_key6);
  EXPECT_EQ(res[1].key, bytes_key7);
  EXPECT_EQ(res[2].key, bytes_key1);
  EXPECT_EQ(res[3].key, bytes_key2);
  EXPECT_EQ(res[4].key, bytes_key3);
  EXPECT_EQ(res[5].key, bytes_key4);
  EXPECT_EQ(res[6].key, bytes_key5);

  EXPECT_EQ(res.size(), 7);
};

TEST(MemtableTest, SearchKey) {
  // Arrange
  std::uint32_t seed{1000};
  Memtable memtable{seed};
  std::string key1{"apple"};
  auto bytes_key1{bytes(key1)};

  // Act
  memtable.insert(bytes_key1, bytes_key1, OperationRecord::PUT, false);
  Node *node{memtable.search(bytes_key1)};
  // Assert
  EXPECT_TRUE(node != nullptr);
  EXPECT_EQ(bytes_key1, node->key);
};

TEST(MemtableTest, SearchKeyAndNotFoundKey) {
  // Arrange
  std::uint32_t seed{1000};
  Memtable memtable{seed};

  std::string key1{"apple"};
  std::string key2{"banana"};

  auto bytes_key1{bytes(key1)};
  auto bytes_key2{bytes(key2)};

  // Act
  memtable.insert(bytes_key1, bytes_key1, OperationRecord::PUT, false);
  Node *node{memtable.search(bytes_key1)};
  Node *node1{memtable.search(bytes_key2)};

  // Assert
  EXPECT_TRUE(node != nullptr);
  EXPECT_EQ(bytes_key1, node->key);
  EXPECT_TRUE(node1 == nullptr);
};

TEST(MemtableTest, MultiLevelStressTest) {
  // Arrange
  const int max_keys{1000};
  const std::uint32_t seed{1000000};
  Memtable memtable{seed};
  std::vector<std::vector<std::byte>> keys_ordered;
  std::vector<std::vector<std::byte>> keys_shuffled;
  keys_ordered.reserve(max_keys);
  keys_shuffled.reserve(max_keys);

  for (int i{}; i < max_keys; i++) {
    std::string key = std::format("key_{:03d}", i);
    std::vector<std::byte> key_byte{bytes(key)};
    keys_ordered.push_back(std::move(key_byte));
  }

  std::mt19937 generator(seed);
  keys_shuffled = keys_ordered;
  std::shuffle(keys_shuffled.begin(), keys_shuffled.end(), generator);

  // Act
  for (auto key : keys_shuffled) {
    memtable.insert(key, key, OperationRecord::PUT, false);
  }

  // Assert
  auto records{memtable.linear_iteration()};

  for (int i{}; i < records.size(); i++) {
    EXPECT_EQ(keys_ordered[i], records[i].key);
  }
};
