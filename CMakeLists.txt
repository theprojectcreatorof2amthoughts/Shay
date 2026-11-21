cmake_minimum_required(VERSION 3.20)
project(Shay VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(shay_frontend STATIC src/lexer.cpp src/parser.cpp src/shui.cpp src/shuil.cpp)
target_include_directories(shay_frontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(shay_runtime STATIC src/runtime.c)
target_include_directories(shay_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(shaii src/shaii.cpp)
target_link_libraries(shaii PRIVATE shay_frontend shay_runtime)

add_executable(gshai src/gshai.cpp)
target_link_libraries(gshai PRIVATE shay_frontend)

add_executable(shui src/shui_main.cpp)
target_link_libraries(shui PRIVATE shay_frontend)
if(WIN32)
    target_sources(shui PRIVATE src/shui_win32.cpp)
    target_link_libraries(shui PRIVATE user32 gdi32)
endif()

foreach(target shaii gshai shui shay_frontend shay_runtime)
    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wall;-Wextra;-Wpedantic>
        $<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-Wall;-Wextra;-Wpedantic>
    )
endforeach()

enable_testing()
add_test(NAME shay_toolchain_suite
         COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/run.sh
                 $<TARGET_FILE:shaii> $<TARGET_FILE:gshai> $<TARGET_FILE:shui>)
set_tests_properties(shay_toolchain_suite PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
