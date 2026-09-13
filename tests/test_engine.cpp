#include "LsmEngine.h"
#include "engine_op.h"
#include <cstddef>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

class EngineTest : public ::testing::Test {
protected:
  fs::path dir_path;
  void SetUp() override {
    dir_path =
        fs::temp_directory_path() /
        ("engine_" + std::to_string(::getpid()) + "-" +
         ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::create_directory(dir_path);
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
