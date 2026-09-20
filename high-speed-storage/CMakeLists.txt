cmake_minimum_required(VERSION 3.16)
project(high-speed-storage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hss src/main.cpp)
target_include_directories(hss PRIVATE src)

add_executable(hss-test tests/test.cpp)
target_include_directories(hss-test PRIVATE src)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  foreach(tgt hss hss-test)
    target_compile_options(${tgt} PRIVATE -O3 -Wall -Wextra -Wpedantic)
  endforeach()
endif()
