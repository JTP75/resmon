add_executable(resmon_tests
  unit/test_scaffold.cpp
  unit/test_config.cpp
  unit/test_prometheus.cpp
  unit/test_rapl_delta.cpp
  unit/test_cpu_load.cpp
  unit/test_cpu_temp_parse.cpp
  unit/test_json_schema.cpp
)
target_include_directories(resmon_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(resmon_tests PRIVATE resmon_core GTest::gtest_main)
target_compile_definitions(resmon_tests PRIVATE
  RESMON_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

include(GoogleTest)
gtest_discover_tests(resmon_tests)
