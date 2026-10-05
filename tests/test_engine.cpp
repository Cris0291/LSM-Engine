#include "LsmEngine.h"
#include "engine_op.h"
#include "lsm_utilities.h"
#include "memtable.h"
#include "operation.h"
#include "sstable_writer.h"
#include <cstddef>
#include <filesystem>
#include <format>
#include <gtest/gtest.h>
#include <gtest/gtest_prod.h>
#include <optional>
#include <string>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

class EngineTest : public ::testing::Test {
protected:
  fs::path dir_path;
  fs::path sstable_dir;
  fs::path sstable0_file;
  fs::path sstable1_file;
  fs::path sstable2_file;
  fs::path sstable3_file;

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
    sstable0_file = sstable_dir / "test0.sst";
    sstable1_file = sstable_dir / "test1.sst";
    sstable2_file = sstable_dir / "test2.sst";
    sstable3_file = sstable_dir / "test3.sst";
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

static void insert_engine_table(LsmEngine &engine, int i) {
  std::size_t max_keys{10000};

  for (; i < max_keys; i++) {
    std::vector<std::byte> key{bytes(std::format("key_{:06d}", i))};
    engine.put(key, key);
  }
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
  Memtable memtable0{seed};
  Memtable memtable1{seed};
  Memtable memtable2{seed};
  Memtable memtable3{seed};

  std::vector<std::byte> key1{bytes("key1")};
  std::vector<std::byte> key2{bytes("key2")};
  std::vector<std::byte> key3{bytes("key3")};
  std::vector<std::byte> key4{bytes("key4")};
  std::vector<std::byte> key5{bytes("key5")};
  std::vector<std::byte> key6{bytes("key6")};
  std::vector<std::byte> key7{bytes("key7")};

  std::string key1_L1{"apple"};
  std::string key2_L1{"banana"};
  std::string key3_L1{"cat"};

  std::string key4_L1{"door"};
  std::string key5_L1{"ether"};
  std::string key6_L1{"flag"};

  std::string key7_L1{"great"};
  std::string key8_L1{"holy"};
  std::string key9_L1{"imm"};

  std::string key10_L1{"joke"};
  std::string key11_L1{"kino"};
  std::string key12_L1{"like"};

  auto bytes_key1{bytes(key1_L1)};
  auto bytes_key2{bytes(key2_L1)};
  auto bytes_key3{bytes(key3_L1)};

  auto bytes_key4{bytes(key4_L1)};
  auto bytes_key5{bytes(key5_L1)};
  auto bytes_key6{bytes(key6_L1)};

  auto bytes_key7{bytes(key7_L1)};
  auto bytes_key8{bytes(key8_L1)};
  auto bytes_key9{bytes(key9_L1)};

  auto bytes_key10{bytes(key10_L1)};
  auto bytes_key11{bytes(key11_L1)};
  auto bytes_key12{bytes(key12_L1)};

  memtable0.insert(bytes_key1, bytes_key1, OperationRecord::PUT, false);
  memtable0.insert(bytes_key2, bytes_key2, OperationRecord::PUT, false);
  memtable0.insert(bytes_key3, bytes_key2, OperationRecord::PUT, false);

  memtable1.insert(bytes_key4, bytes_key4, OperationRecord::PUT, false);
  memtable1.insert(bytes_key5, bytes_key5, OperationRecord::PUT, false);
  memtable1.insert(bytes_key6, bytes_key6, OperationRecord::PUT, false);

  memtable2.insert(bytes_key7, bytes_key7, OperationRecord::PUT, false);
  memtable2.insert(bytes_key8, bytes_key8, OperationRecord::PUT, false);
  memtable2.insert(bytes_key9, bytes_key9, OperationRecord::PUT, false);

  memtable3.insert(bytes_key10, bytes_key10, OperationRecord::PUT, false);
  memtable3.insert(bytes_key11, bytes_key11, OperationRecord::PUT, false);
  memtable3.insert(bytes_key12, bytes_key12, OperationRecord::PUT, false);

  SstableWriter sstable_writer0{SstableWriter(sstable0_file)};
  sstable_writer0.flush_memtable(memtable0);
  auto sstable0{std::make_shared<Sstable>(sstable0_file)};
  sstable0->set_min_max(sstable_writer0.min_key, sstable_writer0.max_key);

  SstableWriter sstable_writer1{SstableWriter(sstable1_file)};
  sstable_writer1.flush_memtable(memtable1);
  auto sstable1{std::make_shared<Sstable>(sstable1_file)};
  sstable1->set_min_max(sstable_writer1.min_key, sstable_writer1.max_key);

  SstableWriter sstable_writer2{SstableWriter(sstable2_file)};
  sstable_writer2.flush_memtable(memtable2);
  auto sstable2{std::make_shared<Sstable>(sstable2_file)};
  sstable2->set_min_max(sstable_writer2.min_key, sstable_writer2.max_key);

  SstableWriter sstable_writer3{SstableWriter(sstable3_file)};
  sstable_writer3.flush_memtable(memtable3);
  auto sstable3{std::make_shared<Sstable>(sstable3_file)};
  sstable3->set_min_max(sstable_writer3.min_key, sstable_writer3.max_key);

  // Act
  engine.put(key1, key1);
  engine.put(key2, key2);
  engine.put(key3, key3);
  engine.put(key4, key4);
  engine.put(key5, key5);
  engine.put(key6, key6);
  engine.put(key7, key7);

  engine.add_to_level(1, std::move(sstable0));
  engine.add_to_level(1, std::move(sstable1));
  engine.add_to_level(1, std::move(sstable2));
  engine.add_to_level(1, std::move(sstable3));

  // Assert
  std::optional<std::vector<std::byte>> res1{engine.get(key1)};
  std::optional<std::vector<std::byte>> res2{engine.get(bytes_key10)};

  EXPECT_TRUE(res1.has_value());
  EXPECT_TRUE(res2.has_value());
  EXPECT_EQ(res1.value(), key1);
  EXPECT_EQ(res2.value(), bytes_key10);
}

TEST_F(EngineTest, KWayMerge) {
  // Arrange
  std::size_t threshold{650};
  MemoryUnit unit{MemoryUnit::B};
  LsmEngine engine{LsmEngine(dir_path, threshold, unit)};

  std::vector<std::byte> key1{bytes("key1")};
  std::vector<std::byte> key2{bytes("key2")};
  std::vector<std::byte> key3{bytes("key3")};
  std::vector<std::byte> key4{bytes("key4")};
  std::vector<std::byte> key5{bytes("key5")};
  std::vector<std::byte> key6{bytes("key7")};
  std::vector<std::byte> key7{bytes("key7")};
  std::vector<std::byte> key8{bytes("key7")};
  std::vector<std::byte> key9{bytes("key8")};
  std::vector<std::byte> key10{bytes("key9")};
  std::vector<std::byte> key11{bytes("key10")};
  std::vector<std::byte> key12{bytes("key11")};
  std::vector<std::byte> key13{bytes("key11")};
  std::vector<std::byte> key14{bytes("key11")};
  std::vector<std::byte> key15{bytes("key11")};
  std::vector<std::byte> key16{bytes("key12")};
  std::vector<std::byte> key17{bytes("key12")};
  std::vector<std::byte> key18{bytes("key12")};
  std::vector<std::byte> key19{bytes("key13")};
  std::vector<std::byte> key20{bytes("key13")};
  std::vector<std::byte> key21{bytes("key13")};
  std::vector<std::byte> key22{bytes("key13")};

  // Act
  engine.put(key1, key1);
  engine.put(key2, key2);
  engine.put(key3, key3);
  engine.put(key4, key4);
  engine.put(key5, key5);
  engine.put(key6, key6);
  engine.put(key7, key7);
  engine.put(key8, key8);
  engine.put(key9, key9);
  engine.put(key10, key10);
  engine.put(key11, key11);
  engine.put(key12, key12);
  engine.put(key13, key13);
  engine.put(key14, key14);
  engine.put(key15, key15);
  engine.put(key16, key16);
  engine.put(key17, key17);
  engine.put(key18, key18);
  engine.put(key19, key19);
  engine.put(key20, key20);
  engine.put(key21, key21);
  engine.put(key22, key22);

  // Assert
  std::vector<Record> res{engine.merge_tables(engine.records[0], false)};
  for (int i{1}; i < res.size(); ++i) {
    int prev{i - 1};
    int compare_res{compare_bytes(res[prev].key, res[i].key)};
    EXPECT_LT(compare_res, 0);
  }
}
