cmake_minimum_required(VERSION 3.24)
project(hmbtn_port LANGUAGES C)

include(CTest)
option(HMBTN_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Only 64-bit native targets are supported")
endif()
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "Use Clang, Apple Clang, or GCC for the native port")
endif()

add_library(hmbtn_port_options INTERFACE)
target_compile_features(hmbtn_port_options INTERFACE c_std_11)
target_include_directories(hmbtn_port_options INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(hmbtn_port_options INTERFACE
    -Wall -Wextra -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks
    -fvisibility=hidden "$<$<CONFIG:Release>:-O2>")
if(HMBTN_SANITIZERS)
    target_compile_options(hmbtn_port_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(hmbtn_port_options INTERFACE -fsanitize=address,undefined)
endif()

# Phase 0 validates the build boundary; it does not compile the game yet.
if(BUILD_TESTING)
    add_executable(hmbtn_types_test tests/types.c)
    target_link_libraries(hmbtn_types_test PRIVATE hmbtn_port_options)
    add_test(NAME compat.types COMMAND hmbtn_types_test)
endif()
