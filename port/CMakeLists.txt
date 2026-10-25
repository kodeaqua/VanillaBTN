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
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/main/geometry")
# Original headers refer to one another by basename. Keep the matching tree read-only.
file(GLOB_RECURSE hm_game_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../src/*.h")
set(hm_game_include_dirs)
foreach(header IN LISTS hm_game_headers)
    get_filename_component(header_directory "${header}" DIRECTORY)
    list(APPEND hm_game_include_dirs "${header_directory}")
endforeach()
list(REMOVE_DUPLICATES hm_game_include_dirs)
target_include_directories(hmbtn_game_options INTERFACE ${hm_game_include_dirs})
target_compile_options(hmbtn_game_options INTERFACE
    -include "${CMAKE_CURRENT_SOURCE_DIR}/compat/game.h")
target_compile_definitions(hmbtn_game_options INTERFACE HMBTN_PORT=1)

add_library(hmbtn_game_initial STATIC
    ../src/main/main.c
    ../src/main/cd/afile.c
    ../src/main/geometry/math_util.c
    ../src/main/game/farm_data_size.c
    ../src/main/game/horse_names.c
    ../src/main/game/event_flags.c
    ../src/main/game/farm_tables.c
    ../src/main/game/picture_codes.c)
target_link_libraries(hmbtn_game_initial PUBLIC hmbtn_game_options)
# Keep the game's entry point separate from the host test runners. This source
# is compiled into the archive only; no native game executable is linked yet.
set_source_files_properties(../src/main/main.c PROPERTIES COMPILE_DEFINITIONS main=hm_game_main)

add_library(hmbtn_psx_initial STATIC
    psx/unimplemented.c psx/gte_stubs.c psx/cd_position.c psx/cd_stubs.c)
target_link_libraries(hmbtn_psx_initial PUBLIC hmbtn_port_options)
target_include_directories(hmbtn_psx_initial PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/compat/psyq")

if(BUILD_TESTING)
    add_executable(hmbtn_types_test tests/types.c)
    target_link_libraries(hmbtn_types_test PRIVATE hmbtn_port_options)
    add_test(NAME compat.types COMMAND hmbtn_types_test)
    add_executable(hmbtn_game_math_test tests/game_math.c)
    target_link_libraries(hmbtn_game_math_test PRIVATE hmbtn_game_initial hmbtn_psx_initial)
    add_test(NAME game.math COMMAND hmbtn_game_math_test)
    add_executable(hmbtn_game_state_test tests/game_state.c tests/item_callback_traps.c)
    target_link_libraries(hmbtn_game_state_test PRIVATE hmbtn_game_initial hmbtn_psx_initial)
    add_test(NAME game.state COMMAND hmbtn_game_state_test)
    add_executable(hmbtn_layouts_test tests/layouts.c)
    target_link_libraries(hmbtn_layouts_test PRIVATE hmbtn_game_options)
    add_test(NAME compat.layouts COMMAND hmbtn_layouts_test)
    add_executable(hmbtn_cd_position_test tests/cd_position.c)
    target_link_libraries(hmbtn_cd_position_test PRIVATE hmbtn_psx_initial)
    add_test(NAME cd.position COMMAND hmbtn_cd_position_test)
    add_executable(hmbtn_cd_stub_test tests/cd_stub.c)
    target_link_libraries(hmbtn_cd_stub_test PRIVATE hmbtn_psx_initial)
    foreach(api IN ITEMS CdInit CdSearchFile CdControl CdRead CdReadSync CdSync CdReadBreak CdFlush)
        add_test(NAME psx.unimplemented.${api}
            COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:hmbtn_cd_stub_test>"
                "-DAPI=${api}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_unimplemented.cmake")
    endforeach()
    add_executable(hmbtn_gte_stub_test tests/gte_stub.c)
    target_link_libraries(hmbtn_gte_stub_test PRIVATE hmbtn_psx_initial)
    foreach(api IN ITEMS rsin rcos SquareRoot0 VectorNormal)
        add_test(NAME psx.unimplemented.${api}
            COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:hmbtn_gte_stub_test>"
                "-DAPI=${api}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_unimplemented.cmake")
    endforeach()
endif()
