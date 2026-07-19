cmake_minimum_required(VERSION 3.20)
project(resmon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(RESMON_BUILD_TESTS "Build resmon unit/integration tests" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(MOSQUITTO REQUIRED IMPORTED_TARGET libmosquitto)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)

add_library(resmon_core
  src/config.cpp
  src/http_client.cpp
  src/prometheus.cpp
  src/fsutil.cpp
  src/command_runner.cpp
  src/proc_locator.cpp
  src/mqtt_publisher.cpp
  src/collectors/cpu_collector.cpp
  src/collectors/mem_collector.cpp
  src/collectors/gpu_collector.cpp
  src/collectors/llama_collector.cpp
)
target_include_directories(resmon_core PUBLIC include)
target_link_libraries(resmon_core PUBLIC
  PkgConfig::CURL
  PkgConfig::MOSQUITTO
  nlohmann_json::nlohmann_json
  Threads::Threads
)

add_executable(resmon src/main.cpp)
target_link_libraries(resmon PRIVATE resmon_core)

if(RESMON_BUILD_TESTS)
  enable_testing()
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
  )
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  add_subdirectory(tests)
endif()
