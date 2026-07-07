#include "wal.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <system_error>
#include <unistd.h>

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

namespace fs = std::filesystem;

class WalTest : public ::testing::Test {
protected:
  fs::path dir_path;
  fs::path file_path;

public:
  void SetUp() override {
    dir_path =
        fs::temp_directory_path() /
        fs::path(
            "wal_test_" + std::to_string(::getpid()) + "_" +
            ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::create_directories(dir_path);
    file_path = dir_path / "test.wal";
  };

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(dir_path, ec);
  };
};

TEST_F(WalTest, RoundTripWalSingleRecord) {
  // Arrange
  OperationRecord op{OperationRecord::PUT};
  std::string key{"key_record"};
  std::string value{"value_record"};
  auto key_bytes{bytes(key)};
  auto value_bytes{bytes(value)};

  // Act
  {
    Wal wal(dir_path.c_str(), file_path.c_str());
    wal.append(op, key_bytes, value_bytes);
  }

  Wal wal2(dir_path.c_str(), file_path.c_str());
  std::vector<Record> res{wal2.replay_whole_file()};

  // Assert
  EXPECT_EQ(res.size(), 1);
  EXPECT_EQ(res[0].op, op);
  EXPECT_TRUE(res[0].key == key_bytes);
  EXPECT_TRUE(res[0].value == value_bytes);
};
