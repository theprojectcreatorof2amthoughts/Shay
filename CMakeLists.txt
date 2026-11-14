cmake_minimum_required(VERSION 3.20)
project(Shay VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(shay_runtime STATIC src/runtime/shay_runtime.c)
target_include_directories(shay_runtime PUBLIC src/runtime)

add_executable(shay src/frontend/main.cpp)
target_link_libraries(shay PRIVATE shay_runtime)

if(MSVC)
  target_compile_options(shay_runtime PRIVATE /W4)
  target_compile_options(shay PRIVATE /W4)
else()
  target_compile_options(shay_runtime PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(shay PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME shay_help COMMAND shay --help)
