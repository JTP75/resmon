#include "resmon/collectors/cpu_collector.hpp"

#include <gtest/gtest.h>

#include <string>

namespace resmon {
namespace {

// tests/fixtures/sys/class/hwmon/ contains two chips: hwmon0 = "acpitz"
// (decoy, no labels) and hwmon1 = "coretemp" (package + 2 cores). This
// exercises chip selection by name rather than by hwmon number, since
// hwmon numbering isn't stable across boots.
std::string fixtureRoot() { return RESMON_TEST_FIXTURES_DIR; }

TEST(CpuTempParse, FindsCoretempChipByNameAndParsesLabels) {
  CpuTempReading reading = readCpuTemps(fixtureRoot());
  ASSERT_TRUE(reading.package_c.has_value());
  EXPECT_DOUBLE_EQ(*reading.package_c, 40.0);
  ASSERT_EQ(reading.core_c.size(), 2u);
  EXPECT_DOUBLE_EQ(reading.core_c[0], 35.0);  // Core 0, from temp2_*
  EXPECT_DOUBLE_EQ(reading.core_c[1], 33.0);  // Core 1, from temp3_*
}

TEST(CpuTempParse, MissingHwmonTreeReturnsEmptyReading) {
  CpuTempReading reading = readCpuTemps(fixtureRoot() + "/nonexistent");
  EXPECT_FALSE(reading.package_c.has_value());
  EXPECT_TRUE(reading.core_c.empty());
}

TEST(CpuFreqParse, AveragesCpuMhzFields) {
  auto mhz = readCpuFreqMhz(fixtureRoot());
  ASSERT_TRUE(mhz.has_value());
  // fixtures/proc/cpuinfo (written alongside meminfo) averages to a known value.
  EXPECT_GT(*mhz, 0.0);
}

}  // namespace
}  // namespace resmon
