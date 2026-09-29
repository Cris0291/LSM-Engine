#include "LsmEngine.h"
#include "engine_op.h"
#include "memtable.h"
#include "sstable.h"
#include "sstable_writer.h"
#include <cstddef>
#include <filesystem>
#include <gtest/gtest.h>
#include <gtest/gtest_prod.h>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

class EngineTest : public ::testing::Test {
protected:
  fs::path dir_path;
  fs::path sstable_dir;
  fs::path sstable_file;
  void SetUp() override {
    dir_path =
        fs::temp_directory_path() /
        ("engine_" + std::to_string(::getpid()) + "-" +
         ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::create_directory(dir_path);

    sstable_dir =
        fs::temp_directory_path() /
        ("sstable_" + std::to_string(::getppid()) + "_" +
         ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::create_directory(sstable_dir);
    sstable_file = sstable_dir / "test.sst";
  }
  void TearDown() override {
    std::error_code ec;
    std::error_code ec1;
    fs::remove_all(dir_path, ec);
    fs::remove_all(sstable_dir, ec1);
  }
};

static std::vector<std::byte> bytes(const std::string s) {
  std::vector<std::byte> bytes;

  for (char c : s) {
    bytes.push_back(static_cast<std::byte>(c));
  }

  return bytes;
};

TEST_F(EngineTest, PutGetOperation) {
  // Arrange
  std::size_t threshold{100};
  MemoryUnit unit{MemoryUnit::MB};
  LsmEngine engine{LsmEngine(dir_path, threshold, unit)};
  std::vector<std::byte> key{bytes("TEST_KEY")};

  // Act
  engine.put(key, key);
  auto res{engine.get(key)};

  // Assert
  EXPECT_TRUE(res.has_value());
  EXPECT_EQ(res.value(), key);
}

TEST_F(EngineTest, PutDeleteGetOperation) {
  // Arrange
  std::size_t threshold{100};
  MemoryUnit unit{MemoryUnit::MB};
  LsmEngine engine{LsmEngine(dir_path, threshold, unit)};
  std::vector<std::byte> get_key{bytes("get_key")};
  std::vector<std::byte> deleted_key{bytes("deleted_key")};

  // Act
  engine.put(get_key, get_key);
  engine.put(deleted_key, deleted_key);
  engine.delete_record(deleted_key);

  auto res_get{engine.get(get_key)};
  auto res_deleted{engine.get(deleted_key)};

  // Assert
  EXPECT_TRUE(res_get.has_value());
  EXPECT_FALSE(res_deleted.has_value());
  EXPECT_EQ(res_get.value(), get_key);
  EXPECT_EQ(res_deleted, std::nullopt);
}

TEST_F(EngineTest, PutFlushGetOperation) {
  // Arrange
  std::size_t threshold{1};
  MemoryUnit unit{MemoryUnit::B};
  LsmEngine engine{LsmEngine(dir_path, threshold, unit)};
  std::vector<std::byte> key1{bytes("key1")};
  std::vector<std::byte> key2{bytes("key2")};

  // Act
  engine.put(key1, key1);
  engine.put(key2, key2);

  auto res_key1{engine.get(key1)};
  auto res_key2{engine.get(key2)};

  // Assert
  EXPECT_EQ(res_key1.value(), key1);
  EXPECT_EQ(res_key2.value(), key2);
}

TEST_F(EngineTest, PutFlushDeleteGetOperation) {
  // Arrange
  std::size_t threshold{650};
  MemoryUnit unit{MemoryUnit::B};
  LsmEngine engine{LsmEngine(dir_path, threshold, unit)};
  std::vector<std::byte> key1{bytes("key1")};
  std::vector<std::byte> key2{bytes("key2")};
  std::vector<std::byte> key3{bytes("key3")};
  std::vector<std::byte> key4{bytes("key4")};
  std::vector<std::byte> key5{bytes("key5")};
  std::vector<std::byte> key6{bytes("key6")};
  std::vector<std::byte> key7{bytes("key7")};

  // Act
  engine.put(key1, key1);
  engine.put(key2, key2);
  engine.put(key3, key3);
  engine.put(key4, key4);
  engine.put(key5, key5);
  engine.put(key6, key6);
  engine.put(key7, key7);

  engine.delete_record(key1);

  auto res_key1{engine.get(key1)};

  // Assert
  EXPECT_FALSE(res_key1.has_value());
  EXPECT_EQ(res_key1, std::nullopt);
}

TEST_F(EngineTest, ReconstructOperation) {
  // Arrange
  std::size_t threshold{1};
  MemoryUnit unit{MemoryUnit::B};
  std::vector<std::byte> key1{bytes("key1")};
  std::vector<std::byte> key2{bytes("key2")};
  std::vector<std::byte> key3{bytes("key3")};
  std::vector<std::byte> key4{bytes("key4")};
  std::vector<std::byte> key5{bytes("key5")};
  std::vector<std::byte> key6{bytes("key6")};
  std::vector<std::byte> key7{bytes("key7")};

  // Act
  {
    LsmEngine engine{LsmEngine(dir_path, threshold, unit)};
    engine.put(key1, key1);
    engine.put(key2, key2);
    engine.put(key3, key3);
    engine.put(key4, key4);
    engine.put(key5, key5);
    engine.put(key6, key6);
    engine.put(key7, key7);
  }

  LsmEngine engine{LsmEngine(dir_path, threshold, unit)};
  auto res_key1{engine.get(key1)};

  // Assert
  EXPECT_TRUE(res_key1.has_value());
  EXPECT_EQ(res_key1.value(), key1);
}

TEST_F(EngineTest, LevelReadPath) {
  // Arrange
  std::size_t threshold{650};
  MemoryUnit unit{MemoryUnit::B};
  LsmEngine engine{LsmEngine(dir_path, threshold, unit)};

  std::uint32_t seed{1042};
  Memtable memtable{seed};

  std::vector<std::byte> key1{bytes("key1")};
  std::vector<std::byte> key2{bytes("key2")};
  std::vector<std::byte> key3{bytes("key3")};
  std::vector<std::byte> key4{bytes("key4")};
  std::vector<std::byte> key5{bytes("key5")};
  std::vector<std::byte> key6{bytes("key6")};
  std::vector<std::byte> key7{bytes("key7")};

  std::string key1_L1{"apple"};
  std::string key2_L1{"banana"};

  auto bytes_key1{bytes(key1_L1)};
  auto bytes_key2{bytes(key2_L1)};

  memtable.insert(bytes_key1, bytes_key1, OperationRecord::PUT, false);
  memtable.insert(bytes_key2, bytes_key2, OperationRecord::PUT, false);

  SstableWriter sstable_writer{SstableWriter(sstable_file)};
  auto sstable{std::make_shared<Sstable>(sstable_file)};

  // Act
  engine.put(key1, key1);
  engine.put(key2, key2);
  engine.put(key3, key3);
  engine.put(key4, key4);
  engine.put(key5, key5);
  engine.put(key6, key6);
  engine.put(key7, key7);

  engine.add_to_level(1, std::move(sstable));

  // Assert
  std::optional<std::vector<std::byte>> res1{engine.get(key1)};
  std::optional<std::vector<std::byte>> res2{engine.get(bytes_key1)};

  EXPECT_TRUE(res1.has_value());
  EXPECT_TRUE(res2.has_value());
  EXPECT_EQ(res1.value(), key1);
  EXPECT_EQ(res2.value(), bytes_key1);
}
