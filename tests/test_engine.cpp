#include "LsmEngine.h"
#include "engine_op.h"
#include "lsm_utilities.h"
#include "memtable.h"
#include "operation.h"
#include "sstable.h"
#include "sstable_writer.h"
#include <cstddef>
#include <filesystem>
#include <format>
#include <gtest/gtest.h>
#include <gtest/gtest_prod.h>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <tuple>
#include <unistd.h>
#include <vector>

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

static std::string from_bytes_to_string(std::vector<std::byte> bytes) {
  std::string s;
  for (std::byte b : bytes) {
    s.push_back(static_cast<char>(b));
  }

  return s;
}

static std::shared_ptr<Sstable>
create_table(const fs::path &file,
             std::vector<std::tuple<std::string, std::string, OperationRecord>>
                 &entries) {
  std::uint32_t seed{1042};
  Memtable memtable{seed};

  for (auto &[k, v, op] : entries) {
    memtable.insert(bytes(k), bytes(v), op, false);
  }

  SstableWriter sswriter{SstableWriter(file)};
  sswriter.flush_memtable(memtable);
  auto sstable{std::make_shared<Sstable>(file)};
  sstable->min_key = sswriter.min_key;
  sstable->max_key = sswriter.max_key;

  return sstable;
}

static void fill_vector(
    std::vector<std::tuple<std::string, std::string, OperationRecord>> &vec) {
  std::size_t max_keys{1000};

  for (int i{}; i < max_keys; i++) {
    std::string key{std::format("key_{:06d}", i)};
    vec.push_back({key, key, OperationRecord::PUT});
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

TEST_F(EngineTest, MergeWithManyTables) {
  // Arrange
  std::size_t threshold{650};
  MemoryUnit unit{MemoryUnit::B};
  LsmEngine engine{LsmEngine(dir_path, threshold, unit)};

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

  std::string key13_L1{"lion"};
  std::string key14_L1{"machine"};
  std::string key15_L1{"hike"};

  std::string key16_L1{"orns"};
  std::string key17_L1{"pops"};
  std::string key18_L1{"itchs"};
  std::string key19_L1{"circle"};
  std::string key20_L1{"lens"};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries1{
      {key1_L1, key1_L1, OperationRecord::PUT},
      {key2_L1, key2_L1, OperationRecord::PUT},
      {key3_L1, key3_L1, OperationRecord::PUT},
      {key4_L1, key4_L1, OperationRecord::PUT},
      {key5_L1, key5_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries2{
      {key6_L1, key6_L1, OperationRecord::PUT},
      {key7_L1, key7_L1, OperationRecord::PUT},
      {key8_L1, key8_L1, OperationRecord::PUT},
      {key9_L1, key9_L1, OperationRecord::PUT},
      {key10_L1, key10_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries3{
      {key11_L1, key11_L1, OperationRecord::PUT},
      {key12_L1, key12_L1, OperationRecord::PUT},
      {key13_L1, key13_L1, OperationRecord::PUT},
      {key14_L1, key14_L1, OperationRecord::PUT},
      {key15_L1, key15_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries4{
      {key16_L1, key16_L1, OperationRecord::PUT},
      {key17_L1, key17_L1, OperationRecord::PUT},
      {key18_L1, key7_L1, OperationRecord::PUT},
      {key19_L1, key18_L1, OperationRecord::PUT},
      {key20_L1, key20_L1, OperationRecord::PUT}};

  auto sstable1{create_table(sstable0_file, entries1)};
  auto sstable2{create_table(sstable1_file, entries2)};
  auto sstable3{create_table(sstable2_file, entries3)};
  auto sstable4{create_table(sstable3_file, entries4)};

  // Act
  engine.add_to_level(0, std::move(sstable1));
  engine.add_to_level(0, std::move(sstable2));
  engine.add_to_level(0, std::move(sstable3));
  engine.add_to_level(0, std::move(sstable4));

  std::vector<Record> records{engine.merge_tables(engine.records[0], false)};

  // Assert

  for (int i{1}; i < records.size(); ++i) {
    int prev{i - 1};
    int res{compare_bytes(records[prev].key, records[i].key)};
    EXPECT_LT(res, 0);
  }
}

TEST_F(EngineTest, MergeNewestWinsWithManyTables) {
  // Arrange
  std::size_t threshold{650};
  MemoryUnit unit{MemoryUnit::B};
  LsmEngine engine{LsmEngine(dir_path, threshold, unit)};

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

  std::string key13_L1{"lion"};
  std::string key14_L1{"machine"};
  std::string key15_L1{"hike"};

  std::string key16_L1{"orns"};
  std::string key17_L1{"pops"};
  std::string key18_L1{"itchs"};
  std::string key19_L1{"circle"};
  std::string key20_L1{"lens"};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries1{
      {key1_L1, key1_L1, OperationRecord::PUT},
      {key2_L1, key2_L1, OperationRecord::PUT},
      {key3_L1, key3_L1, OperationRecord::PUT},
      {key4_L1, key4_L1, OperationRecord::PUT},
      {key5_L1, key5_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries2{
      {key6_L1, key6_L1, OperationRecord::PUT},
      {key7_L1, key7_L1, OperationRecord::PUT},
      {key8_L1, key8_L1, OperationRecord::PUT},
      {key9_L1, key9_L1, OperationRecord::PUT},
      {key10_L1, key10_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries3{
      {key11_L1, key11_L1, OperationRecord::PUT},
      {key12_L1, key12_L1, OperationRecord::PUT},
      {key13_L1, key13_L1, OperationRecord::PUT},
      {key14_L1, key14_L1, OperationRecord::PUT},
      {key15_L1, key15_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries4{
      {key16_L1, key16_L1, OperationRecord::PUT},
      {key17_L1, key17_L1, OperationRecord::PUT},
      {key7_L1, key7_L1, OperationRecord::PUT},
      {key18_L1, key18_L1, OperationRecord::PUT},
      {key20_L1, key20_L1, OperationRecord::PUT}};

  auto sstable1{create_table(sstable0_file, entries1)};
  auto sstable2{create_table(sstable1_file, entries2)};
  auto sstable3{create_table(sstable2_file, entries3)};
  auto sstable4{create_table(sstable3_file, entries4)};

  // Act
  engine.add_to_level(0, std::move(sstable1));
  engine.add_to_level(0, std::move(sstable2));
  engine.add_to_level(0, std::move(sstable3));
  engine.add_to_level(0, std::move(sstable4));

  std::vector<Record> records{engine.merge_tables(engine.records[0], false)};

  // Assert
  std::vector<std::byte> test_key{bytes(key7_L1)};
  int key_count{};
  for (auto &record : records) {
    int res{compare_bytes(record.key, test_key)};
    if (res == 0) {
      key_count += 1;
    }
  }

  EXPECT_EQ(key_count, 1);

  for (int i{1}; i < records.size(); ++i) {
    int prev{i - 1};
    int res{compare_bytes(records[prev].key, records[i].key)};
    EXPECT_LT(res, 0);
  }
}

TEST_F(EngineTest, TombstoneLast) {
  // Arrange
  std::size_t threshold{650};
  MemoryUnit unit{MemoryUnit::B};
  LsmEngine engine{LsmEngine(dir_path, threshold, unit)};

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

  std::string key13_L1{"lion"};
  std::string key14_L1{"machine"};
  std::string key15_L1{"hike"};

  std::string key16_L1{"orns"};
  std::string key17_L1{"pops"};
  std::string key18_L1{"itchs"};
  std::string key19_L1{"circle"};
  std::string key20_L1{"lens"};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries1{
      {key1_L1, key1_L1, OperationRecord::PUT},
      {key2_L1, key2_L1, OperationRecord::PUT},
      {key3_L1, key3_L1, OperationRecord::PUT},
      {key4_L1, key4_L1, OperationRecord::PUT},
      {key5_L1, key5_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries2{
      {key6_L1, key6_L1, OperationRecord::PUT},
      {key7_L1, key7_L1, OperationRecord::DELETE},
      {key8_L1, key8_L1, OperationRecord::PUT},
      {key9_L1, key9_L1, OperationRecord::PUT},
      {key10_L1, key10_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries3{
      {key11_L1, key11_L1, OperationRecord::PUT},
      {key12_L1, key12_L1, OperationRecord::PUT},
      {key13_L1, key13_L1, OperationRecord::PUT},
      {key14_L1, key14_L1, OperationRecord::PUT},
      {key15_L1, key15_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries4{
      {key16_L1, key16_L1, OperationRecord::PUT},
      {key17_L1, key17_L1, OperationRecord::PUT},
      {key7_L1, key7_L1, OperationRecord::PUT},
      {key18_L1, key18_L1, OperationRecord::PUT},
      {key20_L1, key20_L1, OperationRecord::PUT}};

  auto sstable1{create_table(sstable0_file, entries1)};
  auto sstable2{create_table(sstable1_file, entries2)};
  auto sstable3{create_table(sstable2_file, entries3)};
  auto sstable4{create_table(sstable3_file, entries4)};

  // Act
  engine.add_to_level(0, std::move(sstable1));
  engine.add_to_level(0, std::move(sstable2));
  engine.add_to_level(0, std::move(sstable3));
  engine.add_to_level(0, std::move(sstable4));

  std::vector<Record> records{engine.merge_tables(engine.records[0], true)};

  // Assert
  for (int i{1}; i < records.size(); ++i) {
    int prev{i - 1};
    int res{compare_bytes(records[prev].key, records[i].key)};
    EXPECT_LT(res, 0);
  }

  bool is_deleted_key{false};
  for (auto &record : records) {
    std::string del_key{from_bytes_to_string(record.key)};
    if (del_key == key7_L1) {
      is_deleted_key = true;
    }
  }

  EXPECT_FALSE(is_deleted_key);
}

TEST_F(EngineTest, TombstoneNotLast) {
  // Arrange
  std::size_t threshold{650};
  MemoryUnit unit{MemoryUnit::B};
  LsmEngine engine{LsmEngine(dir_path, threshold, unit)};

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

  std::string key13_L1{"lion"};
  std::string key14_L1{"machine"};
  std::string key15_L1{"hike"};

  std::string key16_L1{"orns"};
  std::string key17_L1{"pops"};
  std::string key18_L1{"itchs"};
  std::string key19_L1{"circle"};
  std::string key20_L1{"lens"};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries1{
      {key1_L1, key1_L1, OperationRecord::PUT},
      {key2_L1, key2_L1, OperationRecord::PUT},
      {key3_L1, key3_L1, OperationRecord::PUT},
      {key4_L1, key4_L1, OperationRecord::PUT},
      {key5_L1, key5_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries2{
      {key6_L1, key6_L1, OperationRecord::PUT},
      {key7_L1, key7_L1, OperationRecord::DELETE},
      {key8_L1, key8_L1, OperationRecord::PUT},
      {key9_L1, key9_L1, OperationRecord::PUT},
      {key10_L1, key10_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries3{
      {key11_L1, key11_L1, OperationRecord::PUT},
      {key12_L1, key12_L1, OperationRecord::PUT},
      {key13_L1, key13_L1, OperationRecord::PUT},
      {key14_L1, key14_L1, OperationRecord::PUT},
      {key15_L1, key15_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries4{
      {key16_L1, key16_L1, OperationRecord::PUT},
      {key17_L1, key17_L1, OperationRecord::PUT},
      {key7_L1, key7_L1, OperationRecord::PUT},
      {key18_L1, key18_L1, OperationRecord::PUT},
      {key20_L1, key20_L1, OperationRecord::PUT}};

  auto sstable1{create_table(sstable0_file, entries1)};
  auto sstable2{create_table(sstable1_file, entries2)};
  auto sstable3{create_table(sstable2_file, entries3)};
  auto sstable4{create_table(sstable3_file, entries4)};

  // Act
  engine.add_to_level(0, std::move(sstable1));
  engine.add_to_level(0, std::move(sstable2));
  engine.add_to_level(0, std::move(sstable3));
  engine.add_to_level(0, std::move(sstable4));

  std::vector<Record> records{engine.merge_tables(engine.records[0], false)};

  // Assert
  for (int i{1}; i < records.size(); ++i) {
    int prev{i - 1};
    int res{compare_bytes(records[prev].key, records[i].key)};
    EXPECT_LT(res, 0);
  }

  bool is_deleted_key{false};
  for (auto &record : records) {
    std::string del_key{from_bytes_to_string(record.key)};
    if (del_key == key7_L1) {
      is_deleted_key = true;
    }
  }

  EXPECT_TRUE(is_deleted_key);
}

TEST_F(EngineTest, SplitMergeIsEqual) {
  // Arrange
  std::size_t threshold{650};
  MemoryUnit unit{MemoryUnit::B};
  LsmEngine engine{LsmEngine(dir_path, threshold, unit, 50)};
  std::vector<Record> res{};

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

  std::string key13_L1{"lion"};
  std::string key14_L1{"machine"};
  std::string key15_L1{"hike"};

  std::string key16_L1{"orns"};
  std::string key17_L1{"pops"};
  std::string key18_L1{"itchs"};
  std::string key19_L1{"circle"};
  std::string key20_L1{"lens"};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries1{
      {key1_L1, key1_L1, OperationRecord::PUT},
      {key2_L1, key2_L1, OperationRecord::PUT},
      {key3_L1, key3_L1, OperationRecord::PUT},
      {key4_L1, key4_L1, OperationRecord::PUT},
      {key5_L1, key5_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries2{
      {key6_L1, key6_L1, OperationRecord::PUT},
      {key7_L1, key7_L1, OperationRecord::PUT},
      {key8_L1, key8_L1, OperationRecord::PUT},
      {key9_L1, key9_L1, OperationRecord::PUT},
      {key10_L1, key10_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries3{
      {key11_L1, key11_L1, OperationRecord::PUT},
      {key12_L1, key12_L1, OperationRecord::PUT},
      {key13_L1, key13_L1, OperationRecord::PUT},
      {key14_L1, key14_L1, OperationRecord::PUT},
      {key15_L1, key15_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries4{
      {key16_L1, key16_L1, OperationRecord::PUT},
      {key17_L1, key17_L1, OperationRecord::PUT},
      {key19_L1, key7_L1, OperationRecord::PUT},
      {key18_L1, key18_L1, OperationRecord::PUT},
      {key20_L1, key20_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries5{};
  fill_vector(entries5);

  auto sstable1{create_table(sstable0_file, entries1)};
  auto sstable2{create_table(sstable1_file, entries2)};
  auto sstable3{create_table(sstable2_file, entries3)};
  auto sstable4{create_table(sstable3_file, entries5)};

  // Act
  engine.add_to_level(0, std::move(sstable1));
  engine.add_to_level(0, std::move(sstable2));
  engine.add_to_level(0, std::move(sstable3));
  engine.add_to_level(0, std::move(sstable4));

  std::vector<Record> records{engine.merge_tables(engine.records[0], false)};

  std::vector<std::shared_ptr<Sstable>> tables{engine.split_merge(records)};

  for (auto &table : tables) {
    auto table_records{table->linera_iteration()};
    res.insert(res.end(), table_records.begin(), table_records.end());
  }

  // Assert
  EXPECT_EQ(records.size(), res.size());

  for (int i{}; i < records.size(); ++i) {
    int comp_res{compare_bytes(records[i].key, res[i].key)};
    EXPECT_EQ(comp_res, 0);
  }
}

TEST_F(EngineTest, SplitMergeIsDisjoint) {
  // Arrange
  std::size_t threshold{650};
  MemoryUnit unit{MemoryUnit::B};
  LsmEngine engine{LsmEngine(dir_path, threshold, unit, 50)};
  std::vector<Record> res{};

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

  std::string key13_L1{"lion"};
  std::string key14_L1{"machine"};
  std::string key15_L1{"hike"};

  std::string key16_L1{"orns"};
  std::string key17_L1{"pops"};
  std::string key18_L1{"itchs"};
  std::string key19_L1{"circle"};
  std::string key20_L1{"lens"};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries1{
      {key1_L1, key1_L1, OperationRecord::PUT},
      {key2_L1, key2_L1, OperationRecord::PUT},
      {key3_L1, key3_L1, OperationRecord::PUT},
      {key4_L1, key4_L1, OperationRecord::PUT},
      {key5_L1, key5_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries2{
      {key6_L1, key6_L1, OperationRecord::PUT},
      {key7_L1, key7_L1, OperationRecord::PUT},
      {key8_L1, key8_L1, OperationRecord::PUT},
      {key9_L1, key9_L1, OperationRecord::PUT},
      {key10_L1, key10_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries3{
      {key11_L1, key11_L1, OperationRecord::PUT},
      {key12_L1, key12_L1, OperationRecord::PUT},
      {key13_L1, key13_L1, OperationRecord::PUT},
      {key14_L1, key14_L1, OperationRecord::PUT},
      {key15_L1, key15_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries4{
      {key16_L1, key16_L1, OperationRecord::PUT},
      {key17_L1, key17_L1, OperationRecord::PUT},
      {key19_L1, key7_L1, OperationRecord::PUT},
      {key18_L1, key18_L1, OperationRecord::PUT},
      {key20_L1, key20_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries5{};
  fill_vector(entries5);

  auto sstable1{create_table(sstable0_file, entries1)};
  auto sstable2{create_table(sstable1_file, entries2)};
  auto sstable3{create_table(sstable2_file, entries3)};
  auto sstable4{create_table(sstable3_file, entries5)};

  // Act
  engine.add_to_level(0, std::move(sstable1));
  engine.add_to_level(0, std::move(sstable2));
  engine.add_to_level(0, std::move(sstable3));
  engine.add_to_level(0, std::move(sstable4));

  std::vector<Record> records{engine.merge_tables(engine.records[0], false)};

  std::vector<std::shared_ptr<Sstable>> tables{engine.split_merge(records)};

  // Assert
  std::vector<std::byte> prev_max{};
  for (int i{1}; i < tables.size(); ++i) {
    int prev{i - 1};
    int first_table_comp{
        compare_bytes(tables[prev]->min_key, tables[prev]->max_key)};
    int second_table_comp{
        compare_bytes(tables[i]->min_key, tables[i]->max_key)};
    int inter_table_comp{
        compare_bytes(tables[prev]->max_key, tables[i]->min_key)};

    EXPECT_LT(first_table_comp, 0);
    EXPECT_LT(second_table_comp, 0);
    EXPECT_LT(inter_table_comp, 0);
  }
}

TEST_F(EngineTest, SplitMergeIsOrdered) {
  // Arrange
  std::size_t threshold{650};
  MemoryUnit unit{MemoryUnit::B};
  LsmEngine engine{LsmEngine(dir_path, threshold, unit, 50)};
  std::vector<Record> res{};

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

  std::string key13_L1{"lion"};
  std::string key14_L1{"machine"};
  std::string key15_L1{"hike"};

  std::string key16_L1{"orns"};
  std::string key17_L1{"pops"};
  std::string key18_L1{"itchs"};
  std::string key19_L1{"circle"};
  std::string key20_L1{"lens"};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries1{
      {key1_L1, key1_L1, OperationRecord::PUT},
      {key2_L1, key2_L1, OperationRecord::PUT},
      {key3_L1, key3_L1, OperationRecord::PUT},
      {key4_L1, key4_L1, OperationRecord::PUT},
      {key5_L1, key5_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries2{
      {key6_L1, key6_L1, OperationRecord::PUT},
      {key7_L1, key7_L1, OperationRecord::PUT},
      {key8_L1, key8_L1, OperationRecord::PUT},
      {key9_L1, key9_L1, OperationRecord::PUT},
      {key10_L1, key10_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries3{
      {key11_L1, key11_L1, OperationRecord::PUT},
      {key12_L1, key12_L1, OperationRecord::PUT},
      {key13_L1, key13_L1, OperationRecord::PUT},
      {key14_L1, key14_L1, OperationRecord::PUT},
      {key15_L1, key15_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries4{
      {key16_L1, key16_L1, OperationRecord::PUT},
      {key17_L1, key17_L1, OperationRecord::PUT},
      {key19_L1, key7_L1, OperationRecord::PUT},
      {key18_L1, key18_L1, OperationRecord::PUT},
      {key20_L1, key20_L1, OperationRecord::PUT}};

  std::vector<std::tuple<std::string, std::string, OperationRecord>> entries5{};
  fill_vector(entries5);

  auto sstable1{create_table(sstable0_file, entries1)};
  auto sstable2{create_table(sstable1_file, entries2)};
  auto sstable3{create_table(sstable2_file, entries3)};
  auto sstable4{create_table(sstable3_file, entries5)};

  // Act
  engine.add_to_level(0, std::move(sstable1));
  engine.add_to_level(0, std::move(sstable2));
  engine.add_to_level(0, std::move(sstable3));
  engine.add_to_level(0, std::move(sstable4));

  std::vector<Record> records{engine.merge_tables(engine.records[0], false)};

  std::vector<std::shared_ptr<Sstable>> tables{engine.split_merge(records)};

  for (auto &table : tables) {
    auto table_records{table->linera_iteration()};
    res.insert(res.end(), table_records.begin(), table_records.end());
  }

  // Assert
  EXPECT_EQ(records.size(), res.size());

  for (int i{1}; i < res.size(); ++i) {
    int prev{i - 1};
    int comp_res{compare_bytes(res[prev].key, res[i].key)};
    EXPECT_LT(comp_res, 0);
  }
}
