add_executable(resmon_tests
  unit/test_scaffold.cpp
  unit/test_config.cpp
  unit/test_prometheus.cpp
)
target_include_directories(resmon_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(resmon_tests PRIVATE resmon_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(resmon_tests)
