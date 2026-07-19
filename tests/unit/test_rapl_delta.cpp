#include "resmon/collectors/cpu_collector.hpp"

#include <gtest/gtest.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace resmon {
namespace {

namespace fs = std::filesystem;

class RaplDeltaTest : public ::testing::Test {
 protected:
  fs::path root;
  fs::path pkg_dir;

  void SetUp() override {
    root = fs::temp_directory_path() /
           ("resmon_rapl_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
            "_" + std::to_string(getpid()) + "_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
    pkg_dir = root / "sys/class/powercap/intel-rapl:0";
    fs::create_directories(pkg_dir);
  }

  void TearDown() override { fs::remove_all(root); }

  void writeEnergy(long long uj) {
    std::ofstream(pkg_dir / "energy_uj") << uj;
  }
  void writeMaxRange(long long uj) {
    std::ofstream(pkg_dir / "max_energy_range_uj") << uj;
  }
};

TEST_F(RaplDeltaTest, FirstSampleReturnsNullopt) {
  writeEnergy(1'000'000);
  writeMaxRange(1'000'000'000);
  RaplReader reader(root.string());
  EXPECT_FALSE(reader.samplePowerWatts().has_value());
}

TEST_F(RaplDeltaTest, SecondSampleComputesPositivePowerFromDelta) {
  writeMaxRange(1'000'000'000);
  writeEnergy(1'000'000);
  RaplReader reader(root.string());
  ASSERT_FALSE(reader.samplePowerWatts().has_value());

  writeEnergy(2'000'000);  // +1,000,000 uJ = +1 J
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto p = reader.samplePowerWatts();
  ASSERT_TRUE(p.has_value());
  EXPECT_GT(*p, 0.0);
  EXPECT_LT(*p, 1000.0);  // sanity bound; 1J over >=50ms is well under 1kW
}

TEST_F(RaplDeltaTest, WraparoundStillYieldsPositivePower) {
  writeMaxRange(1'000'000);  // small range so the counter wraps easily
  writeEnergy(900'000);
  RaplReader reader(root.string());
  ASSERT_FALSE(reader.samplePowerWatts().has_value());

  // Counter wrapped past max back down to 100,000: true delta is
  // (1,000,000 - 900,000) + 100,000 = 200,000 uJ, not a negative number.
  writeEnergy(100'000);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto p = reader.samplePowerWatts();
  ASSERT_TRUE(p.has_value());
  EXPECT_GT(*p, 0.0);
}

TEST_F(RaplDeltaTest, MissingEnergyFileReturnsNullopt) {
  RaplReader reader(root.string());  // energy_uj never written
  EXPECT_FALSE(reader.samplePowerWatts().has_value());
}

}  // namespace
}  // namespace resmon
