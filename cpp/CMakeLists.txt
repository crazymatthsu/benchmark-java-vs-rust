cmake_minimum_required(VERSION 3.16)
project(cpp-bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(cpp-bench src/main.cpp)
target_include_directories(cpp-bench PRIVATE src)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cpp-bench PRIVATE -O3 -flto -Wall -Wextra -Wpedantic)
  target_link_options(cpp-bench PRIVATE -flto)
endif()
