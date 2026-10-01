cmake_minimum_required(VERSION 3.16)
project(energy_agent CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(Threads REQUIRED)

add_executable(energy-agent src/main.cpp)
target_include_directories(energy-agent PRIVATE include)
target_link_libraries(energy-agent PRIVATE Threads::Threads)
