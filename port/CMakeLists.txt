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

add_library(hmbtn_work_memory STATIC compat/work_memory.c compat/cpu.c)
target_link_libraries(hmbtn_work_memory PUBLIC hmbtn_port_options)

# Compile original translation units with port-only compatibility hooks.
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
    ../src/main/graphics/frame_buffers.c
    ../src/main/graphics/prim.c
    ../src/main/graphics/texture_pack.c
    ../src/main/geometry/math_util.c
    ../src/main/geometry/ground.c
    ../src/overlay/11_GF_FARM/animals/animals.c
    ../src/overlay/11_GF_FARM/estate.c
    ../src/overlay/11_GF_FARM/menu/shop_dialogues.c
    ../src/overlay/11_GF_FARM/event/event_rules.c
    ../src/main/sound/cd_audio.c
    ../src/main/sound/sound.c
    ../src/main/sound/sound_request.c
    ../src/overlay/08_DEBUG/debug.c
    ../src/overlay/11_GF_FARM/event/event_commands.c
    ../src/overlay/11_GF_FARM/people/player.c
    ../src/overlay/11_GF_FARM/world/scene_view.c
    ../src/overlay/21_GF_MG3/mg3.c
    ../src/overlay/25_GF_MG5/mg5.c
    ../src/main/game/farm_data_size.c
    ../src/main/game/farm_state.c
    ../src/main/game/horse_names.c
    ../src/main/game/event_flags.c
    ../src/main/game/farm_tables.c
    ../src/main/game/picture_codes.c)
target_link_libraries(hmbtn_game_initial PUBLIC hmbtn_game_options hmbtn_work_memory)
# Keep the game's entry point separate from the host test runners. This source
# is compiled into the archive only; no native game executable is linked yet.
set_source_files_properties(../src/main/main.c PROPERTIES COMPILE_DEFINITIONS main=hm_game_main)

add_library(hmbtn_psx_initial STATIC
    psx/unimplemented.c psx/gte_stubs.c psx/cd_position.c psx/cd_runtime.c
    psx/gpu_links.c psx/gpu_ot.c psx/gpu_packets.c psx/vram.c)
target_link_libraries(hmbtn_psx_initial PUBLIC hmbtn_port_options)
target_include_directories(hmbtn_psx_initial PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/compat/psyq")

add_library(hmbtn_disc STATIC psx/disc.c psx/sha1.c)
target_link_libraries(hmbtn_disc PUBLIC hmbtn_port_options)
target_link_libraries(hmbtn_psx_initial PUBLIC hmbtn_disc)
add_executable(hmbtn_disc_probe platform/disc_probe.c)
target_link_libraries(hmbtn_disc_probe PRIVATE hmbtn_psx_initial)

add_executable(hmbtn_ot_bench EXCLUDE_FROM_ALL analysis/ot_bench.c)
target_link_libraries(hmbtn_ot_bench PRIVATE hmbtn_psx_initial)

if(BUILD_TESTING)
    add_executable(hmbtn_nested_helpers_test tests/nested_helpers.c)
    target_link_libraries(hmbtn_nested_helpers_test PRIVATE hmbtn_game_options)
    add_test(NAME compat.nested_helpers COMMAND hmbtn_nested_helpers_test)
    add_executable(hmbtn_gte_core_test tests/gte_core.c)
    target_link_libraries(hmbtn_gte_core_test PRIVATE hmbtn_psx_initial)
    add_test(NAME gte.core COMMAND hmbtn_gte_core_test)
    add_executable(hmbtn_sha1_test tests/sha1.c)
    target_link_libraries(hmbtn_sha1_test PRIVATE hmbtn_disc)
    add_test(NAME disc.sha1 COMMAND hmbtn_sha1_test)
    add_executable(hmbtn_disc_test tests/disc.c)
    target_link_libraries(hmbtn_disc_test PRIVATE hmbtn_disc)
    add_test(NAME disc.parser COMMAND hmbtn_disc_test)
    add_executable(hmbtn_cd_runtime_test tests/cd_runtime.c)
    target_link_libraries(hmbtn_cd_runtime_test PRIVATE hmbtn_psx_initial)
    add_test(NAME cd.runtime COMMAND hmbtn_cd_runtime_test)
    add_executable(hmbtn_vram_test tests/vram.c)
    target_link_libraries(hmbtn_vram_test PRIVATE hmbtn_psx_initial)
    add_test(NAME gpu.vram COMMAND hmbtn_vram_test)
    add_executable(hmbtn_texture_test tests/texture_pack.c)
    target_link_libraries(hmbtn_texture_test PRIVATE hmbtn_game_initial hmbtn_psx_initial)
    add_test(NAME game.texture_upload COMMAND hmbtn_texture_test)
    add_executable(hmbtn_types_test tests/types.c)
    target_link_libraries(hmbtn_types_test PRIVATE hmbtn_port_options)
    add_test(NAME compat.types COMMAND hmbtn_types_test)
    add_executable(hmbtn_game_math_test tests/game_math.c)
    target_link_libraries(hmbtn_game_math_test PRIVATE hmbtn_game_initial hmbtn_psx_initial)
    add_test(NAME game.math COMMAND hmbtn_game_math_test)
    add_executable(hmbtn_game_state_test tests/game_state.c tests/item_callback_traps.c)
    target_link_libraries(hmbtn_game_state_test PRIVATE hmbtn_game_initial hmbtn_psx_initial)
    add_test(NAME game.state COMMAND hmbtn_game_state_test)
    add_executable(hmbtn_work_memory_test tests/work_memory.c tests/item_callback_traps.c)
    target_link_libraries(hmbtn_work_memory_test PRIVATE hmbtn_game_initial hmbtn_psx_initial)
    # This fixture includes the original main.c to exercise its static setup
    # functions. Discard the unreachable game entry and its unfinished dependencies.
    target_compile_options(hmbtn_work_memory_test PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(hmbtn_work_memory_test PRIVATE
        "$<$<PLATFORM_ID:Windows,Linux,Android>:-Wl,--gc-sections>"
        "$<$<PLATFORM_ID:Darwin>:-Wl,-dead_strip>")
    add_test(NAME game.work_memory COMMAND hmbtn_work_memory_test)
    add_executable(hmbtn_gpu_links_test tests/gpu_links.c)
    target_link_libraries(hmbtn_gpu_links_test PRIVATE hmbtn_game_initial hmbtn_psx_initial)
    add_test(NAME gpu.links COMMAND hmbtn_gpu_links_test)
    add_executable(hmbtn_gpu_packets_test tests/gpu_packets.c)
    target_link_libraries(hmbtn_gpu_packets_test PRIVATE hmbtn_psx_initial)
    add_test(NAME gpu.packets COMMAND hmbtn_gpu_packets_test)
    foreach(rejection IN ITEMS null unaligned capacity unknown zero empty)
        if(rejection MATCHES "^(null|unaligned|capacity)$")
            set(context hm_gpu_link_encode)
            if(rejection STREQUAL "capacity")
                set(detail "handle capacity exceeded")
            else()
                set(detail "${rejection} tag")
            endif()
        elseif(rejection STREQUAL "empty")
            set(context ClearOTagR)
            set(detail "non-positive length")
        else()
            set(context hm_gpu_link_decode)
            set(detail "invalid handle")
        endif()
        add_test(NAME gpu.links.reject.${rejection}
            COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:hmbtn_gpu_links_test>"
                "-DCASE=${rejection}" "-DCONTEXT=${context}" "-DDETAIL=${detail}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_runtime_error.cmake")
    endforeach()
    add_executable(hmbtn_layouts_test tests/layouts.c)
    target_link_libraries(hmbtn_layouts_test PRIVATE hmbtn_game_options)
    add_test(NAME compat.layouts COMMAND hmbtn_layouts_test)
    add_executable(hmbtn_cd_position_test tests/cd_position.c)
    target_link_libraries(hmbtn_cd_position_test PRIVATE hmbtn_psx_initial)
    add_test(NAME cd.position COMMAND hmbtn_cd_position_test)
    add_executable(hmbtn_cd_stub_test tests/cd_stub.c)
    target_link_libraries(hmbtn_cd_stub_test PRIVATE hmbtn_psx_initial)
    foreach(api IN ITEMS CdInit CdSearchFile CdControl CdRead CdReadSync CdSync CdReadBreak CdFlush)
        add_test(NAME cd.unmounted.${api}
            COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:hmbtn_cd_stub_test>"
                "-DCASE=${api}" "-DCONTEXT=${api}" "-DDETAIL=no validated disc is attached"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_runtime_error.cmake")
    endforeach()
    add_executable(hmbtn_gte_stub_test tests/gte_stub.c)
    target_link_libraries(hmbtn_gte_stub_test PRIVATE hmbtn_psx_initial)
    foreach(api IN ITEMS rsin rcos SquareRoot0 VectorNormal)
        add_test(NAME psx.unimplemented.${api}
            COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:hmbtn_gte_stub_test>"
                "-DAPI=${api}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_unimplemented.cmake")
    endforeach()
endif()
