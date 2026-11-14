cmake_minimum_required(VERSION 3.20)
project(Shay VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(shay_runtime STATIC src/runtime.c)
target_include_directories(shay_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(gshai src/gshai.cpp)
target_link_libraries(gshai PRIVATE shay_runtime)
target_compile_options(gshai PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

enable_testing()
add_test(NAME shay_hello_check
         COMMAND gshai --check ${CMAKE_CURRENT_SOURCE_DIR}/examples/hello.shay)
