#include "record-encoder.h"
#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
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
