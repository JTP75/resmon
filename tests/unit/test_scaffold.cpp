#include <gtest/gtest.h>

#include "resmon/constants.hpp"

TEST(Scaffold, SchemaVersionIsPositive) {
  EXPECT_GT(resmon::constants::kSchemaVersion, 0);
}
