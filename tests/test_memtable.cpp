#include "lsm_utilities.h"
#include <cstddef>
#include <gtest/gtest.h>
#include <string>

static std::vector<std::byte> bytes(const std::string &s) {
  std::vector<std::byte> v;
  v.reserve(s.size());
  for (char c : s) {
    v.push_back(static_cast<std::byte>(c));
  }
  return v;
}

TEST(MemtableTest, CompareEqualBytes) {
  // Arrange
  std::string value{"TEST"};
  std::vector<std::byte> value_bytes{bytes(value)};

  // Act
  int res{compare_bytes(value_bytes, value_bytes)};

  // Assert
  EXPECT_EQ(res, 0);
};

TEST(MemtableTest, CompareLessBytes) {
  std::string lex_less{"ab"};
  std::string lex_greater{"az"};

  std::vector<std::byte> bytes_lex_less{bytes(lex_less)};
  std::vector<std::byte> byets_lex_greater{bytes(lex_greater)};
};
