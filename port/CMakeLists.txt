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
    target_compile_options(hmbtn_port_options INTERFACE -fsanitize=address,undefined
        -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_link_options(hmbtn_port_options INTERFACE -fsanitize=address,undefined)
endif()

# Compile original translation units without changing the matching source tree.
add_library(hmbtn_game_options INTERFACE)
target_link_libraries(hmbtn_game_options INTERFACE hmbtn_port_options)
target_include_directories(hmbtn_game_options INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/psyq"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/main/geometry")
target_compile_options(hmbtn_game_options INTERFACE
    -include "${CMAKE_CURRENT_SOURCE_DIR}/compat/types.h")
target_compile_definitions(hmbtn_game_options INTERFACE HMBTN_PORT=1)

add_library(hmbtn_game_initial STATIC
    ../src/main/geometry/math_util.c
    ../src/main/game/farm_data_size.c
    ../src/main/game/horse_names.c
    ../src/main/game/picture_codes.c)
target_link_libraries(hmbtn_game_initial PUBLIC hmbtn_game_options)

add_library(hmbtn_psx_initial STATIC psx/unimplemented.c psx/gte_stubs.c)
target_link_libraries(hmbtn_psx_initial PUBLIC hmbtn_port_options)
target_include_directories(hmbtn_psx_initial PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/compat/psyq")

if(BUILD_TESTING)
    add_executable(hmbtn_types_test tests/types.c)
    target_link_libraries(hmbtn_types_test PRIVATE hmbtn_port_options)
    add_test(NAME compat.types COMMAND hmbtn_types_test)
    add_executable(hmbtn_game_math_test tests/game_math.c)
    target_link_libraries(hmbtn_game_math_test PRIVATE hmbtn_game_initial hmbtn_psx_initial)
    add_test(NAME game.math COMMAND hmbtn_game_math_test)
    add_executable(hmbtn_gte_stub_test tests/gte_stub.c)
    target_link_libraries(hmbtn_gte_stub_test PRIVATE hmbtn_psx_initial)
    foreach(api IN ITEMS rsin rcos SquareRoot0 VectorNormal)
        add_test(NAME psx.unimplemented.${api}
            COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:hmbtn_gte_stub_test>"
                "-DAPI=${api}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_unimplemented.cmake")
    endforeach()
endif()
