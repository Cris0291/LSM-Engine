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

TEST_F(WalTest, CompareVariousRecordsWithBothReplays) {
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
  auto value_byes2{bytes(value2)};

  // Act
  {
    Wal wal(dir_path.c_str(), file_path.c_str());
    wal.append(op, key_bytes, value_bytes);
    wal.append(op1, key_bytes1, value_bytes1);
    wal.append(op2, key_bytes2, value_byes2);
  }

  Wal wal2(dir_path.c_str(), file_path.c_str());
  std::vector<Record> res1{wal2.replay_whole_file()};
  std::vector<Record> res2{wal2.replay()};

  EXPECT_EQ(res1, res2);
  EXPECT_EQ(res1.size(), 3);
  EXPECT_EQ(res2.size(), 3);
}

TEST_F(WalTest, TornTailWithReplayWholeFile) {
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
  auto value_byes2{bytes(value2)};

  // Act
  {
    Wal wal(dir_path.c_str(), file_path.c_str());
    wal.append(op, key_bytes, value_bytes);
    wal.append(op1, key_bytes1, value_bytes1);
    wal.append(op2, key_bytes2, value_byes2);
  }

  std::error_code ec;
  Wal wal2(dir_path.c_str(), file_path.c_str());
  // fs::resize_file(file_path, 4000, ec);
  std::vector<Record> res{wal2.replay()};
  std::cout << "file size o disk" << fs::file_size(file_path) << "\n";
  std::cout << "records recovered" << res.size() << "\n";
  EXPECT_EQ(res.size(), 3);
}
