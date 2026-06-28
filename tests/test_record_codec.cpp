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

TEST(RecordCodec, Truncation) {
  // Arrange
  OperationRecord put{OperationRecord::PUT};
  std::string key_s{"TEST_TRUNCATION"};
  std::string value_s{"VALUE_TRUNCATION"};
  std::vector<std::byte> key_bytes{bytes(key_s)};
  std::vector<std::byte> value_bytes{bytes(value_s)};
  std::size_t header_size{13};
  std::size_t random_less_header{random_between(0, header_size - 1)};
  std::size_t random_truncation{};

  // Act
  auto encode_result{RecordEncoder::encode(put, key_bytes, value_bytes)};
  std::span<std::byte> encode_span(encode_result);
  random_truncation = random_between(header_size, encode_result.size() - 1);
  DecodeResult decode_result_less_header{
      RecordEncoder::decode(encode_span.subspan(0, random_less_header))};
  DecodeResult decode_result_truncation{
      RecordEncoder::decode(encode_span.subspan(0, random_truncation))};

  // Assert
  EXPECT_EQ(decode_result_less_header.status, DecodeStatus::TRUNCATED);
  EXPECT_EQ(decode_result_truncation.status, DecodeStatus::TRUNCATED);
}
