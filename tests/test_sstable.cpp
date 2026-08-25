#include "memtable.h"
#include "sstable.h"
#include "sstable_operation.h"
#include "sstable_writer.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <gtest/gtest.h>
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
  SstableWriter sswriter{file_path, dir_path};
  Memtable memtable{create_memtable()};
  std::vector<Memtable::Record> mem_records{memtable.linear_iteration()};
  sswriter.flush_memtable(memtable);
  Sstable ssreader{file_path};

  // Act
  std::vector<RecordSstable> sstable_records{ssreader.linera_iteration()};

  // Assert
  EXPECT_EQ(mem_records.size(), sstable_records.size());
  for (int i{}; i < mem_records.size(); i++) {
    EXPECT_EQ(mem_records[i].op, sstable_records[i].op);
    EXPECT_EQ(mem_records[i].key, sstable_records[i].key);
    EXPECT_EQ(mem_records[i].value, sstable_records[i].value);
  }
};
