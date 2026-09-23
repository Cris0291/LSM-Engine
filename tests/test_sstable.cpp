#include "memtable.h"
#include "operation.h"
#include "sstable.h"
#include "sstable_writer.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

class SstableTest : public ::testing::Test {
protected:
  fs::path dir_path;
  fs::path file_path;
  void SetUp() override {
    dir_path =
        fs::temp_directory_path() /
        ("sstable_" + std::to_string(::getppid()) + "_" +
         ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::create_directory(dir_path);
    file_path = dir_path / "test.sst";
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(dir_path, ec);
  }
};

static std::vector<std::byte> bytes(const std::string s) {
  std::vector<std::byte> bytes;

  for (char c : s) {
    bytes.push_back(static_cast<std::byte>(c));
  }

  return bytes;
};

static Memtable create_memtable() {
  std::size_t max_keys{10000};
  std::uint32_t seed{1000};
  Memtable memtable{seed};

  for (int i{}; i < max_keys; i++) {
    std::vector<std::byte> key{bytes(std::format("key_{:06d}", i))};
    memtable.insert(key, key, OperationRecord::PUT, false);
  }

  return memtable;
};

TEST_F(SstableTest, RoundTrip) {
  // Arrange
  SstableWriter sswriter{file_path};
  Memtable memtable{create_memtable()};
  std::vector<Record> mem_records{memtable.linear_iteration()};
  sswriter.flush_memtable(memtable);
  Sstable ssreader{file_path};

  // Act
  std::vector<Record> sstable_records{ssreader.linera_iteration()};

  // Assert
  EXPECT_EQ(mem_records.size(), sstable_records.size());
  for (int i{}; i < mem_records.size(); i++) {
    EXPECT_EQ(mem_records[i].op, sstable_records[i].op);
    EXPECT_EQ(mem_records[i].key, sstable_records[i].key);
    EXPECT_EQ(mem_records[i].value, sstable_records[i].value);
  }
};

TEST_F(SstableTest, ReadRecord) {
  // Arrange
  std::size_t max_keys{10000};
  SstableWriter sswriter{file_path};
  Memtable memtable{create_memtable()};
  std::vector<Record> mem_records{memtable.linear_iteration()};
  sswriter.flush_memtable(memtable);
  Sstable ssreader{file_path};
  std::optional<Record> res;

  // Act
  for (int i{}; i < max_keys; i++) {
    std::vector<std::byte> key{bytes(std::format("key_{:06d}", i))};
    res = ssreader.read(key);

    // Assert
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res->key, mem_records[i].key);
    EXPECT_EQ(res->value, mem_records[i].value);
    EXPECT_EQ(res->op, mem_records[i].op);
  }
};

TEST_F(SstableTest, ReadNonExistentRecordBeforeFirst) {
  // Arrange
  SstableWriter sswriter{file_path};
  Memtable memtable{create_memtable()};
  sswriter.flush_memtable(memtable);
  Sstable ssreader{file_path};
  std::vector<std::byte> fake_key{bytes("aaa")};

  /// Act
  std::optional<Record> res{ssreader.read(fake_key)};

  // Assert
  EXPECT_FALSE(res.has_value());
}

TEST_F(SstableTest, ReadNonExistentRecordAfterLast) {
  // Arrange
  SstableWriter sswriter{file_path};
  Memtable memtable{create_memtable()};
  sswriter.flush_memtable(memtable);
  Sstable ssreader{file_path};
  std::vector<std::byte> fake_key{bytes("zzz")};

  /// Act
  std::optional<Record> res{ssreader.read(fake_key)};

  // Assert
  EXPECT_FALSE(res.has_value());
}

TEST_F(SstableTest, ReadNonExistentRecordBetweenKeys) {
  // Arrange
  SstableWriter sswriter{file_path};
  Memtable memtable{create_memtable()};
  sswriter.flush_memtable(memtable);
  Sstable ssreader{file_path};
  std::vector<std::byte> fake_key{bytes("key_00700_7")};

  /// Act
  std::optional<Record> res{ssreader.read(fake_key)};

  // Assert
  EXPECT_FALSE(res.has_value());
}

TEST_F(SstableTest, ReadTombStone) {
  // Arrange
  SstableWriter sswriter{file_path};
  Memtable memtable{create_memtable()};
  std::vector<std::byte> tombstone_key{bytes("key_00700_7")};
  memtable.insert(tombstone_key, tombstone_key, OperationRecord::DELETE, true);
  sswriter.flush_memtable(memtable);
  Sstable ssreader{file_path};

  /// Act
  std::optional<Record> res{ssreader.read(tombstone_key)};

  // Assert
  EXPECT_TRUE(res.has_value());
  EXPECT_EQ(res->key, tombstone_key);
  EXPECT_EQ(res->op, OperationRecord::DELETE);
}

TEST_F(SstableTest, IteratorOracleTest) {
  // Arrange
  SstableWriter sswriter{file_path};
  Memtable memtable{create_memtable()};
  sswriter.flush_memtable(memtable);
  std::shared_ptr<Sstable> ssreader{std::make_shared<Sstable>(file_path)};
  Sstable::Iterator it{ssreader.get()->begin()};

  // Act
  std::vector<Record> records{ssreader.get()->linera_iteration()};

  // Assert
  for (int i{}; i < records.size(); i++) {

    EXPECT_EQ(records[i].key, (*it).key);
    EXPECT_EQ(records[i].value, (*it).value);
    EXPECT_EQ(records[i].op, (*it).op);
    ++it;
  }
}
