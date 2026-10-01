cmake_minimum_required(VERSION 3.16)
project(energy_agent CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# Core code shared by the program and the tests
add_library(agent_core STATIC
    src/Logger.cpp
    src/Config.cpp
    src/AnalyticsEngine.cpp
    src/Storage.cpp
)
target_include_directories(agent_core PUBLIC include)
target_link_libraries(agent_core PUBLIC Threads::Threads SQLite::SQLite3)

# The daemon
add_executable(energy-agent src/main.cpp)
target_link_libraries(energy-agent PRIVATE agent_core)

# Unit tests
enable_testing()
find_package(GTest REQUIRED)
add_executable(agent_tests
    tests/test_energy.cpp
    tests/test_storage.cpp
)
target_link_libraries(agent_tests PRIVATE agent_core GTest::gtest_main)
add_test(NAME agent_tests COMMAND agent_tests)
