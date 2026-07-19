#include "resmon/collectors/cpu_collector.hpp"

#include <gtest/gtest.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>

namespace resmon {
namespace {

namespace fs = std::filesystem;

class CpuLoadTest : public ::testing::Test {
 protected:
  fs::path root;
  fs::path proc_dir;

  void SetUp() override {
    root = fs::temp_directory_path() /
           ("resmon_cpuload_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
            "_" + std::to_string(getpid()) + "_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
    proc_dir = root / "proc";
    fs::create_directories(proc_dir);
  }

  void TearDown() override { fs::remove_all(root); }

  void writeStat(const std::string& content) { std::ofstream(proc_dir / "stat") << content; }
};

TEST_F(CpuLoadTest, FirstSampleReturnsNullopt) {
  writeStat("cpu  100 0 100 800 0 0 0 0 0 0\ncpu0 100 0 100 800 0 0 0 0 0 0\n");
  CpuLoadReader reader(root.string());
  EXPECT_FALSE(reader.sample().has_value());
}

TEST_F(CpuLoadTest, FullyBusyDeltaYields100Percent) {
  writeStat("cpu  1000 0 1000 8000 0 0 0 0 0 0\ncpu0 1000 0 1000 8000 0 0 0 0 0 0\n");
  CpuLoadReader reader(root.string());
  ASSERT_FALSE(reader.sample().has_value());

  // total delta = 2000; idle delta = 0 -> fully busy
  writeStat("cpu  2000 0 2000 8000 0 0 0 0 0 0\ncpu0 2000 0 2000 8000 0 0 0 0 0 0\n");
  auto s = reader.sample();
  ASSERT_TRUE(s.has_value());
  EXPECT_DOUBLE_EQ(s->overall_pct, 100.0);
  ASSERT_EQ(s->per_core_pct.size(), 1u);
  EXPECT_DOUBLE_EQ(s->per_core_pct[0], 100.0);
}

TEST_F(CpuLoadTest, FullyIdleDeltaYieldsZeroPercent) {
  writeStat("cpu  1000 0 1000 8000 0 0 0 0 0 0\n");
  CpuLoadReader reader(root.string());
  ASSERT_FALSE(reader.sample().has_value());

  writeStat("cpu  1000 0 1000 9000 0 0 0 0 0 0\n");  // only idle advanced
  auto s = reader.sample();
  ASSERT_TRUE(s.has_value());
  EXPECT_DOUBLE_EQ(s->overall_pct, 0.0);
}

TEST_F(CpuLoadTest, HalfBusyDeltaYieldsFiftyPercent) {
  writeStat("cpu  1000 0 1000 8000 0 0 0 0 0 0\n");
  CpuLoadReader reader(root.string());
  ASSERT_FALSE(reader.sample().has_value());

  // total delta = (2000+1000+9000) - (1000+1000+8000) = 2000; idle delta = 1000 -> 50% busy
  writeStat("cpu  2000 0 1000 9000 0 0 0 0 0 0\n");
  auto s = reader.sample();
  ASSERT_TRUE(s.has_value());
  EXPECT_DOUBLE_EQ(s->overall_pct, 50.0);
}

TEST_F(CpuLoadTest, MissingStatFileReturnsNullopt) {
  CpuLoadReader reader(root.string());  // proc/stat never written
  EXPECT_FALSE(reader.sample().has_value());
}

}  // namespace
}  // namespace resmon
