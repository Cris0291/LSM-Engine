#include "record-encoder.h"
#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
#include <random>
#include <span>
#include <string>
#include <vector>

static std::vector<std::byte> bytes(const std::string &s) {
  std::vector<std::byte> v;
  v.reserve(s.size());
  for (char c : s) {
    v.push_back(static_cast<std::byte>(c));
  }
  return v;
}

static bool spans_equals(std::span<std::byte> a,
                         const std::vector<std::byte> &b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

static std::size_t random_between(std::size_t n, std::size_t m) {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_int_distribution<std::size_t> dist(n, m);

  return dist(gen);
}

TEST(RecordCodec, RoundTripBasic) {
  // Arrange
  OperationRecord put{OperationRecord::PUT};
  std::string key_s{"TEST"};
  std::string value_s{"VALUE"};
  std::vector<std::byte> key_bytes{bytes(key_s)};
  std::vector<std::byte> value_bytes{bytes(value_s)};

  // Act
  std::vector<std::byte> encode_result{
      RecordEncoder::encode(put, key_bytes, value_bytes)};
  DecodeResult decode_result{RecordEncoder::decode(encode_result)};

  // Assert
  EXPECT_EQ(decode_result.op, put);
  EXPECT_EQ(decode_result.status, DecodeStatus::GOOD);
  EXPECT_TRUE(spans_equals(decode_result.key, key_bytes));
  EXPECT_TRUE(spans_equals(decode_result.value, value_bytes));
  EXPECT_EQ(decode_result.bytes_read, encode_result.size());
}

TEST(RecordCodec, RoundTriMultiple) {
  // Arrange

  // Record 1
  OperationRecord op{OperationRecord::PUT};
  std::string key{"key_record"};
  std::string value{"value_record"};
  auto key_bytes{bytes(key)};
  auto value_bytes{bytes(value)};

  // Record 2
  OperationRecord op1{OperationRecord::PUT};
  std::string key1{""};
  std::string value1{""};
  auto key_bytes1{bytes(key1)};
  auto value_bytes1{bytes(value1)};

  // Record 3
  OperationRecord op2{OperationRecord::PUT};
  std::string key2{"key_large"};
  std::string value2(5000, 'A');
  auto key_bytes2{bytes(key2)};
  auto value_bytes2{bytes(value2)};

  // Act
  std::vector<std::byte> encode_result{
      RecordEncoder::encode(op, key_bytes, value_bytes)};
  DecodeResult decode_result{RecordEncoder::decode(encode_result)};

  std::vector<std::byte> encode_result1{
      RecordEncoder::encode(op1, key_bytes1, value_bytes1)};
  DecodeResult decode_result1{RecordEncoder::decode(encode_result1)};

  std::vector<std::byte> encode_result2{
      RecordEncoder::encode(op2, key_bytes2, value_bytes2)};
  DecodeResult decode_result2{RecordEncoder::decode(encode_result2)};

  // Assert
  EXPECT_EQ(decode_result.op, op);
  EXPECT_EQ(decode_result.status, DecodeStatus::GOOD);
  EXPECT_TRUE(spans_equals(decode_result.key, key_bytes));
  EXPECT_TRUE(spans_equals(decode_result.value, value_bytes));
  EXPECT_EQ(decode_result.bytes_read, encode_result.size());

  EXPECT_EQ(decode_result1.op, op1);
  EXPECT_EQ(decode_result1.status, DecodeStatus::GOOD);
  EXPECT_TRUE(spans_equals(decode_result1.key, key_bytes1));
  EXPECT_TRUE(spans_equals(decode_result1.value, value_bytes1));
  EXPECT_EQ(decode_result1.bytes_read, encode_result1.size());

  EXPECT_EQ(decode_result2.op, op2);
  EXPECT_EQ(decode_result2.status, DecodeStatus::GOOD);
  EXPECT_TRUE(spans_equals(decode_result2.key, key_bytes2));
  EXPECT_TRUE(spans_equals(decode_result2.value, value_bytes2));
  EXPECT_EQ(decode_result2.bytes_read, encode_result2.size());
}

TEST(RecordCodec, Truncation) {
  // Arrange
  OperationRecord put{OperationRecord::PUT};
  std::string key_s{"TEST_TRUNCATION"};
  std::string value_s{"VALUE_TRUNCATION"};
  std::vector<std::byte> key_bytes{bytes(key_s)};
  std::vector<std::byte> value_bytes{bytes(value_s)};

  // Act
  auto encode_result{RecordEncoder::encode(put, key_bytes, value_bytes)};
  std::span<std::byte> encode_span(encode_result);

  // Assert
  for (int n{}; n < encode_result.size(); n++) {
    DecodeResult decode_result{
        RecordEncoder::decode(encode_span.subspan(0, n))};
    EXPECT_EQ(decode_result.status, DecodeStatus::TRUNCATED)
        << "failed truncation at length " << n;
  }
}

TEST(RecordCodec, Corruption) {
  // Arrange
  OperationRecord put{OperationRecord::PUT};
  std::string key_s{"KEY_CORRUPTION"};
  std::string value_s{"VALUE_CORRUPTION"};
  std::vector<std::byte> key_bytes{bytes(key_s)};
  std::vector<std::byte> value_bytes{bytes(value_s)};

  // Act
  std::vector<std::byte> encode_result{
      RecordEncoder::encode(put, key_bytes, value_bytes)};

  // Assert
  for (int i{}; i < encode_result.size(); i++) {
    auto corrupted = encode_result;
    corrupted[i] ^= std::byte{0xFF};
    DecodeResult decode_result{RecordEncoder::decode(corrupted)};
    EXPECT_NE(decode_result.status, DecodeStatus::GOOD)
        << "failed truncation at length " << i;
  }
}
