cmake_minimum_required(VERSION 3.24)
project(VanillaBTNNative LANGUAGES C)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The native bringup currently requires an x64 MSVC toolchain on Windows")
endif()

include(FetchContent)
include(game-includes.cmake)
add_library(game_archive STATIC src/game_archive.c)
set_target_properties(game_archive PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(game_archive PUBLIC src)
target_compile_options(game_archive PRIVATE /W4)
target_compile_definitions(game_archive PRIVATE _CRT_SECURE_NO_WARNINGS)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.zip
    URL_HASH SHA256=9cd42377704398796071b8597cd7e21da254a43bad98c4199739647adc13fa6f
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(SDL3)

add_library(ps1_video STATIC src/ps1_video.c)
set_target_properties(ps1_video PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(ps1_video PUBLIC src)
target_compile_options(ps1_video PRIVATE /W4)

add_library(game_graphics STATIC src/game_graphics.c)
set_target_properties(game_graphics PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(game_graphics PUBLIC src)
target_link_libraries(game_graphics PUBLIC game_archive ps1_video)
target_compile_options(game_graphics PRIVATE /W4)

add_library(ps1_ot STATIC src/ps1_ot.c)
set_target_properties(ps1_ot PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(ps1_ot PUBLIC src)
target_compile_options(ps1_ot PRIVATE /W4)

add_library(ps1_gpu STATIC src/ps1_gpu.c)
set_target_properties(ps1_gpu PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(ps1_gpu PUBLIC src)
target_link_libraries(ps1_gpu PUBLIC ps1_video ps1_ot)
target_compile_options(ps1_gpu PRIVATE /W4)

add_library(ps1_clock STATIC src/ps1_clock.c)
set_target_properties(ps1_clock PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(ps1_clock PUBLIC src)
target_compile_options(ps1_clock PRIVATE /W4)

add_library(ps1_spu STATIC src/ps1_spu.c)
set_target_properties(ps1_spu PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(ps1_spu PUBLIC src)
target_compile_options(ps1_spu PRIVATE /W4)

add_library(ps1_xa STATIC src/ps1_xa.c)
set_target_properties(ps1_xa PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(ps1_xa PUBLIC src)
target_compile_options(ps1_xa PRIVATE /W4)

add_library(ps1_seq STATIC src/ps1_seq.c)
set_target_properties(ps1_seq PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(ps1_seq PUBLIC src)
target_link_libraries(ps1_seq PUBLIC ps1_spu)
target_compile_options(ps1_seq PRIVATE /W4)

add_library(ps1_audio STATIC src/ps1_audio.c)
set_target_properties(ps1_audio PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(ps1_audio PUBLIC src)
target_link_libraries(ps1_audio PUBLIC ps1_spu ps1_xa ps1_seq SDL3::SDL3)
target_compile_options(ps1_audio PRIVATE /W4)

add_library(ps1_card STATIC src/ps1_card.c)
set_target_properties(ps1_card PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(ps1_card PUBLIC src)
target_link_libraries(ps1_card PUBLIC SDL3::SDL3)
target_compile_options(ps1_card PRIVATE /W4)

# A named local candidate can be built while another runtime is in use.
set(VANILLABTN_EXECUTABLE_NAME "vanillabtn" CACHE STRING "Native runtime output name")
if(NOT VANILLABTN_EXECUTABLE_NAME MATCHES "^[A-Za-z0-9][A-Za-z0-9_-]*$")
    message(FATAL_ERROR "VANILLABTN_EXECUTABLE_NAME must be a simple executable name")
endif()
add_executable(vanillabtn WIN32 src/main.c)
set_target_properties(vanillabtn PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES
    OUTPUT_NAME "${VANILLABTN_EXECUTABLE_NAME}")
target_link_libraries(vanillabtn PRIVATE SDL3::SDL3 ps1_video ps1_input ps1_clock ps1_audio ps1_card)
target_compile_options(vanillabtn PRIVATE /W4)

enable_testing()
add_library(game_texture_pack STATIC ../src/main/graphics/texture_pack.c)
set_target_properties(game_texture_pack PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(game_texture_pack PUBLIC compat ../src/include ../src/main/graphics)
target_compile_definitions(game_texture_pack PUBLIC VANILLABTN_NATIVE)
# Private unused original helpers reference animation functions not ported yet.
# Separate functions let the linker discard those helpers without fake stubs.
target_compile_options(game_texture_pack PRIVATE /W4 /Gy)

add_library(sdk_gpu STATIC src/sdk_gpu.c)
set_target_properties(sdk_gpu PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(sdk_gpu PUBLIC compat src)
target_link_libraries(sdk_gpu PUBLIC ps1_video ps1_gpu)
target_compile_options(sdk_gpu PRIVATE /W4)
target_link_libraries(game_texture_pack PUBLIC sdk_gpu)
target_link_options(game_texture_pack INTERFACE /OPT:REF)
target_link_libraries(vanillabtn PRIVATE game_texture_pack)

add_executable(sdk_gpu_test tests/sdk_gpu_test.c)
set_target_properties(sdk_gpu_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(sdk_gpu_test PRIVATE game_texture_pack)
target_compile_options(sdk_gpu_test PRIVATE /W4 /UNDEBUG)
add_test(NAME original_texture_pack COMMAND sdk_gpu_test)

add_library(game_prim STATIC ../src/main/graphics/prim.c src/sdk_error.c)
set_target_properties(game_prim PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(game_prim PUBLIC ../src/main ../src/main/input ../src/main/sound)
target_link_libraries(game_prim PUBLIC game_texture_pack)
target_compile_options(game_prim PRIVATE /W4 /Gy)
add_executable(original_prim_test tests/original_prim_test.c)
set_target_properties(original_prim_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(original_prim_test PRIVATE game_prim)
target_compile_options(original_prim_test PRIVATE /W4 /UNDEBUG)
add_test(NAME original_primitive_packets COMMAND original_prim_test)

add_library(game_anim STATIC ../src/main/graphics/anim.c ../src/main/system.c src/sdk_debug.c)
target_include_directories(game_anim PRIVATE ../src/main/geometry)
set_target_properties(game_anim PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(game_anim PUBLIC game_prim)
target_compile_options(game_anim PRIVATE /W4 /Gy)
add_executable(original_anim_test tests/original_anim_test.c)
set_target_properties(original_anim_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(original_anim_test PRIVATE game_anim)
target_compile_options(original_anim_test PRIVATE /W4 /UNDEBUG)
add_test(NAME original_animation_packets COMMAND original_anim_test)

add_library(ps1_gte STATIC src/ps1_gte.c)
set_target_properties(ps1_gte PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(ps1_gte PUBLIC src)
target_compile_options(ps1_gte PRIVATE /W4)

add_library(sdk_math STATIC src/sdk_math.c)
set_target_properties(sdk_math PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(sdk_math PUBLIC compat src)
target_link_libraries(sdk_math PUBLIC ps1_gte)
target_compile_options(sdk_math PRIVATE /W4)
add_executable(sdk_math_test tests/sdk_math_test.c)
set_target_properties(sdk_math_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(sdk_math_test PRIVATE sdk_math)
target_compile_options(sdk_math_test PRIVATE /W4)
add_test(NAME sdk_fixed_math COMMAND sdk_math_test)

add_library(game_frame_buffers STATIC
    ../src/main/geometry/camera.c
    ../src/main/geometry/ground.c
    ../src/main/geometry/math_util.c
    ../src/main/geometry/model.c
    ../src/main/geometry/vec_length.c
    ../src/main/graphics/clut.c
    ../src/main/graphics/frame_buffers.c
    ../src/main/ui/bg_tile.c
    ../src/main/ui/loading_screen.c
    ../src/main/ui/ui_icons.c
)
set_target_properties(game_frame_buffers PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(game_frame_buffers PUBLIC ${game_include_dirs})
target_link_libraries(game_frame_buffers PUBLIC game_anim sdk_math)
target_compile_options(game_frame_buffers PRIVATE /W4 /Gy)
add_executable(original_frame_test tests/original_frame_test.c)
set_target_properties(original_frame_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(original_frame_test PRIVATE game_frame_buffers)
target_compile_options(original_frame_test PRIVATE /W4 /UNDEBUG)
add_test(NAME original_frame_buffers COMMAND original_frame_test)

add_library(sdk_sync STATIC src/sdk_sync.c)
set_target_properties(sdk_sync PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(sdk_sync PUBLIC compat src)
target_link_libraries(sdk_sync PUBLIC SDL3::SDL3)
target_compile_options(sdk_sync PRIVATE /W4)
add_executable(sdk_sync_test tests/sdk_sync_test.c)
set_target_properties(sdk_sync_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(sdk_sync_test PRIVATE sdk_sync)
target_compile_options(sdk_sync_test PRIVATE /W4 /UNDEBUG)
add_test(NAME sdk_vsync_timing COMMAND sdk_sync_test)

add_library(sdk_sound STATIC src/sdk_sound.c)
set_target_properties(sdk_sound PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(sdk_sound PUBLIC compat src)
target_link_libraries(sdk_sound PUBLIC ps1_seq)
target_compile_options(sdk_sound PRIVATE /W4)

add_library(sdk_cd STATIC src/sdk_cd.c)
set_target_properties(sdk_cd PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(sdk_cd PUBLIC compat src)
target_compile_definitions(sdk_cd PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(sdk_cd PRIVATE /W4)
add_library(game_afile STATIC
    ../src/main/cd/afile.c
    ../src/main/game/event_flags.c
    ../src/main/sound/cd_audio.c
    ../src/main/sound/sound.c
    ../src/main/sound/sound_request.c
)
set_target_properties(game_afile PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(game_afile PUBLIC ${game_include_dirs})
target_link_libraries(game_afile PUBLIC sdk_cd sdk_sound sdk_sync game_screen)
target_compile_options(game_afile PRIVATE /W4 /Gy)
add_executable(original_afile_test tests/original_afile_test.c)
set_target_properties(original_afile_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(original_afile_test PRIVATE game_afile)
target_compile_definitions(original_afile_test PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(original_afile_test PRIVATE /W4 /UNDEBUG)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/sdk-disc-test/STR")
add_test(NAME original_archive_reads COMMAND original_afile_test)
set_tests_properties(original_archive_reads PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/sdk-disc-test" TIMEOUT 10)

add_library(sdk_memory STATIC ../src/main/overlay_regions.c src/sdk_memory.c)
set_target_properties(sdk_memory PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(sdk_memory PUBLIC compat src ../src/include ../src/main)
target_compile_definitions(sdk_memory PRIVATE VANILLABTN_NATIVE)
target_compile_options(sdk_memory PRIVATE /W4)
add_executable(sdk_memory_test tests/sdk_memory_test.c)
set_target_properties(sdk_memory_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(sdk_memory_test PRIVATE sdk_memory)
target_compile_options(sdk_memory_test PRIVATE /W4 /UNDEBUG)
add_test(NAME native_memory_regions COMMAND sdk_memory_test)

add_library(host_display STATIC src/host_display.c)
set_target_properties(host_display PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(host_display PUBLIC src)
target_link_libraries(host_display PUBLIC sdk_gpu SDL3::SDL3)
target_compile_options(host_display PRIVATE /W4)
target_link_libraries(vanillabtn PRIVATE host_display)
add_executable(host_display_test tests/host_display_test.c)
set_target_properties(host_display_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(host_display_test PRIVATE host_display)
target_compile_options(host_display_test PRIVATE /W4 /UNDEBUG)
add_test(NAME native_display_selection COMMAND host_display_test)

add_library(game_screen STATIC
    ../src/main/debug_stubs.c
    ../src/main/game/cooking_recipes.c
    ../src/main/game/farm_data_size.c
    ../src/main/game/farm_state.c
    ../src/main/game/farm_tables.c
    ../src/main/game/horse_names.c
    ../src/main/game/picture_codes.c
    ../src/main/game/shop_restock.c
    ../src/main/graphics/screen_fade.c
    ../src/main/text/text_bank.c
    ../src/overlay/11_GF_FARM/farm_data.c
    ../src/overlay/11_GF_FARM/farm_vars.c
)
set_target_properties(game_screen PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(game_screen PUBLIC ${game_include_dirs})
target_link_libraries(game_screen PUBLIC game_frame_buffers sdk_memory sdk_sync)
target_compile_options(game_screen PRIVATE /W4 /Gy)
add_executable(original_screen_test tests/original_screen_test.c)
set_target_properties(original_screen_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(original_screen_test PRIVATE game_screen)
target_compile_options(original_screen_test PRIVATE /W4 /UNDEBUG)
add_test(NAME original_screen_fades COMMAND original_screen_test)

add_library(sdk_card STATIC src/sdk_card.c)
set_target_properties(sdk_card PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(sdk_card PUBLIC compat src ${game_include_dirs})
target_link_libraries(sdk_card PUBLIC ps1_card)
target_compile_definitions(sdk_card PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(sdk_card PRIVATE /W4)

add_executable(sdk_card_io_test tests/sdk_card_io_test.c)
set_target_properties(sdk_card_io_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(sdk_card_io_test PRIVATE sdk_card)
target_compile_options(sdk_card_io_test PRIVATE /W4)
add_test(NAME sdk_card_bounded_io COMMAND sdk_card_io_test)

add_executable(sdk_pocketstation_test tests/sdk_pocketstation_test.c)
set_target_properties(sdk_pocketstation_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(sdk_pocketstation_test PRIVATE sdk_card)
target_compile_options(sdk_pocketstation_test PRIVATE /W4)
add_test(NAME sdk_absent_pocketstation COMMAND sdk_pocketstation_test)

add_library(game_window_frame STATIC
    ../src/main/text/text.c
    ../src/main/ui/heart.c
    ../src/main/ui/menu_window.c
    ../src/main/ui/name_entry.c
    ../src/main/ui/window_frame.c
    ../src/overlay/08_DEBUG/debug.c
    ../src/overlay/10_PdaSamp/pdasamp.c
    ../src/overlay/11_GF_FARM/menu/item_storage.c
    ../src/overlay/11_GF_FARM/picture_cells.c
    ../src/overlay/17_GF_MG1/mg1.c
    ../src/overlay/19_GF_MG2/mg2.c
    ../src/overlay/21_GF_MG3/mg3.c
    ../src/overlay/23_GF_MG4/mg4.c
    ../src/overlay/25_GF_MG5/mg5.c
    ../src/overlay/27_GF_TITLE/title.c
    ../src/overlay/28_GF_STAFF/staff.c
    ../src/overlay/31_gf_mcard/mcard.c
)
set_target_properties(game_window_frame PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(game_window_frame PUBLIC ${game_include_dirs})
target_link_libraries(game_window_frame PUBLIC game_afile game_pad sdk_card)
target_compile_options(game_window_frame PRIVATE /W4 /Gy)
add_executable(original_window_test tests/original_window_test.c)
set_target_properties(original_window_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(original_window_test PRIVATE game_window_frame)
target_compile_options(original_window_test PRIVATE /W4 /UNDEBUG)
add_test(NAME original_window_packets COMMAND original_window_test)

add_library(game_farm STATIC
    ../src/overlay/11_GF_FARM/animals/animals.c
    ../src/overlay/11_GF_FARM/animals/animal_actions.c
    ../src/overlay/11_GF_FARM/animals/chicken.c
    ../src/overlay/11_GF_FARM/animals/dog.c
    ../src/overlay/11_GF_FARM/animals/horse.c
    ../src/overlay/11_GF_FARM/animals/livestock.c
    ../src/overlay/11_GF_FARM/demo.c
    ../src/overlay/11_GF_FARM/estate.c
    ../src/overlay/11_GF_FARM/event/cue.c
    ../src/overlay/11_GF_FARM/event/cue_history.c
    ../src/overlay/11_GF_FARM/event/event_commands.c
    ../src/overlay/11_GF_FARM/event/event_request.c
    ../src/overlay/11_GF_FARM/event/event_rules.c
    ../src/overlay/11_GF_FARM/event/event_script.c
    ../src/overlay/11_GF_FARM/event/messages.c
    ../src/overlay/11_GF_FARM/event/milestones.c
    ../src/overlay/11_GF_FARM/farm_frame.c
    ../src/overlay/11_GF_FARM/farm_init.c
    ../src/overlay/11_GF_FARM/farm_load.c
    ../src/overlay/11_GF_FARM/farm_main.c
    ../src/overlay/11_GF_FARM/farm_probe.c
    ../src/overlay/11_GF_FARM/field/field.c
    ../src/overlay/11_GF_FARM/field/field_item_requests.c
    ../src/overlay/11_GF_FARM/field/fishing.c
    ../src/overlay/11_GF_FARM/field/greenhouse.c
    ../src/overlay/11_GF_FARM/field/mine.c
    ../src/overlay/11_GF_FARM/field/pond.c
    ../src/overlay/11_GF_FARM/menu/calendar.c
    ../src/overlay/11_GF_FARM/menu/house.c
    ../src/overlay/11_GF_FARM/menu/menus.c
    ../src/overlay/11_GF_FARM/menu/options_menu.c
    ../src/overlay/11_GF_FARM/menu/photo_album.c
    ../src/overlay/11_GF_FARM/menu/shop_dialogues.c
    ../src/overlay/11_GF_FARM/menu/shop_page.c
    ../src/overlay/11_GF_FARM/menu/status_pages.c
    ../src/overlay/11_GF_FARM/menu/storage_swap.c
    ../src/overlay/11_GF_FARM/people/family.c
    ../src/overlay/11_GF_FARM/people/harvest_sprites.c
    ../src/overlay/11_GF_FARM/people/player.c
    ../src/overlay/11_GF_FARM/people/villager.c
    ../src/overlay/11_GF_FARM/world/clock.c
    ../src/overlay/11_GF_FARM/world/clock_events.c
    ../src/overlay/11_GF_FARM/world/doors.c
    ../src/overlay/11_GF_FARM/world/scene.c
    ../src/overlay/11_GF_FARM/world/scene_view.c
    ../src/overlay/11_GF_FARM/world/weather.c
    ../src/overlay/30_GF_swind/swind.c
)
set_target_properties(game_farm PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(game_farm PUBLIC ${game_include_dirs})
target_link_libraries(game_farm PUBLIC game_window_frame)
target_compile_options(game_farm PRIVATE /W4 /Gy)
target_compile_options(game_farm PRIVATE /we4013)

add_library(sdk_pad STATIC src/sdk_pad.c)
set_target_properties(sdk_pad PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(sdk_pad PUBLIC compat src)
target_link_libraries(sdk_pad PUBLIC sdk_sync)
target_compile_options(sdk_pad PRIVATE /W4)
add_library(game_pad STATIC
    ../src/main/input/pad.c
    ../src/main/input/pad_vibration.c
)
set_target_properties(game_pad PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(game_pad PUBLIC ${game_include_dirs})
target_link_libraries(game_pad PUBLIC sdk_pad sdk_math game_prim)
target_compile_options(game_pad PRIVATE /W4)
add_executable(original_pad_test tests/original_pad_test.c)
set_target_properties(original_pad_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(original_pad_test PRIVATE game_pad)
target_compile_options(original_pad_test PRIVATE /W4 /UNDEBUG)
add_test(NAME original_controller_polling COMMAND original_pad_test)
target_link_libraries(vanillabtn PRIVATE sdk_pad)
add_executable(game_graphics_test tests/game_graphics_test.c)
set_target_properties(game_graphics_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(game_graphics_test PRIVATE game_graphics)
target_compile_options(game_graphics_test PRIVATE /W4 /UNDEBUG)
add_test(NAME game_graphics_assets COMMAND game_graphics_test)
add_executable(ps1_seq_test tests/ps1_seq_test.c)
set_target_properties(ps1_seq_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(ps1_seq_test PRIVATE ps1_seq)
target_compile_options(ps1_seq_test PRIVATE /W4 /UNDEBUG)
add_test(NAME ps1_seq_schedule COMMAND ps1_seq_test)
add_executable(game_archive_test tests/game_archive_test.c)
set_target_properties(game_archive_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(game_archive_test PRIVATE game_archive)
target_compile_options(game_archive_test PRIVATE /W4 /UNDEBUG)
target_compile_definitions(game_archive_test PRIVATE _CRT_SECURE_NO_WARNINGS)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/archive-test")
add_test(NAME game_archive_bounds COMMAND game_archive_test)
set_tests_properties(game_archive_bounds PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/archive-test")
add_test(NAME native_startup COMMAND vanillabtn --check-build)
add_executable(ps1_video_test tests/ps1_video_test.c)
set_target_properties(ps1_video_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(ps1_video_test PRIVATE ps1_video)
target_compile_options(ps1_video_test PRIVATE /W4)
add_test(NAME ps1_video_pixels COMMAND ps1_video_test)
add_executable(ps1_sprite_test tests/ps1_sprite_test.c)
set_target_properties(ps1_sprite_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(ps1_sprite_test PRIVATE ps1_video)
target_compile_options(ps1_sprite_test PRIVATE /W4)
add_test(NAME ps1_sprite_pixels COMMAND ps1_sprite_test)
add_executable(ps1_ot_test tests/ps1_ot_test.c)
set_target_properties(ps1_ot_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(ps1_ot_test PRIVATE ps1_ot)
target_compile_options(ps1_ot_test PRIVATE /W4)
add_test(NAME ps1_ot_order COMMAND ps1_ot_test)

add_executable(ps1_gpu_test tests/ps1_gpu_test.c)
set_target_properties(ps1_gpu_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(ps1_gpu_test PRIVATE ps1_gpu)
target_compile_options(ps1_gpu_test PRIVATE /W4)
add_test(NAME ps1_gpu_dispatch COMMAND ps1_gpu_test)

add_executable(ps1_gte_test tests/ps1_gte_test.c)
set_target_properties(ps1_gte_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(ps1_gte_test PRIVATE ps1_gte)
target_compile_options(ps1_gte_test PRIVATE /W4)
add_test(NAME ps1_gte_math COMMAND ps1_gte_test)

add_library(ps1_input STATIC src/ps1_input.c)
set_target_properties(ps1_input PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(ps1_input PUBLIC src)
target_compile_options(ps1_input PRIVATE /W4)

add_executable(ps1_input_test tests/ps1_input_test.c)
set_target_properties(ps1_input_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(ps1_input_test PRIVATE ps1_input)
target_compile_options(ps1_input_test PRIVATE /W4)
add_test(NAME ps1_input_repeat COMMAND ps1_input_test)

add_executable(ps1_clock_test tests/ps1_clock_test.c)
set_target_properties(ps1_clock_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(ps1_clock_test PRIVATE ps1_clock)
target_compile_options(ps1_clock_test PRIVATE /W4)
add_test(NAME ps1_clock_cadence COMMAND ps1_clock_test)

add_executable(ps1_spu_test tests/ps1_spu_test.c)
set_target_properties(ps1_spu_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(ps1_spu_test PRIVATE ps1_spu)
target_compile_options(ps1_spu_test PRIVATE /W4)
add_test(NAME ps1_spu_decode COMMAND ps1_spu_test)

add_executable(ps1_xa_test tests/ps1_xa_test.c)
set_target_properties(ps1_xa_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(ps1_xa_test PRIVATE ps1_xa)
target_compile_options(ps1_xa_test PRIVATE /W4)
add_test(NAME ps1_xa_stream COMMAND ps1_xa_test)

add_executable(ps1_audio_test tests/ps1_audio_test.c)
set_target_properties(ps1_audio_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(ps1_audio_test PRIVATE ps1_audio SDL3::SDL3)
target_compile_options(ps1_audio_test PRIVATE /W4)
add_test(NAME ps1_audio_playback COMMAND ps1_audio_test)

add_executable(ps1_card_test tests/ps1_card_test.c)
set_target_properties(ps1_card_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(ps1_card_test PRIVATE ps1_card SDL3::SDL3)
target_compile_options(ps1_card_test PRIVATE /W4)
add_test(NAME ps1_card_storage COMMAND ps1_card_test)

# Synthetic assertions remain active when testing optimized Release libraries.
foreach(test_target ps1_video_test ps1_sprite_test ps1_ot_test ps1_gpu_test
        ps1_gte_test ps1_input_test ps1_clock_test ps1_spu_test ps1_xa_test
        ps1_audio_test ps1_card_test)
    target_compile_options(${test_target} PRIVATE /UNDEBUG)
endforeach()

# Link the authoritative startup and overlays into the SDL host.
add_library(game_main STATIC ../src/main/main.c)
set_target_properties(game_main PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(game_main PUBLIC compat ${game_include_dirs} src)
target_compile_definitions(game_main PUBLIC VANILLABTN_NATIVE PRIVATE main=game_main)
target_compile_options(game_main PRIVATE /W4 /Gy /we4013)

add_library(game_saveload STATIC ../src/overlay/15_saveload/saveload.c)
set_target_properties(game_saveload PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(game_saveload PUBLIC compat ${game_include_dirs} src)
target_compile_definitions(game_saveload PUBLIC VANILLABTN_NATIVE)
target_compile_options(game_saveload PRIVATE /W4 /Gy /we4013)

target_link_libraries(game_main PUBLIC game_farm game_saveload game_window_frame
    game_afile game_pad sdk_card)

target_link_libraries(vanillabtn PRIVATE game_main)
target_link_libraries(sdk_gpu PUBLIC sdk_math)

# Optional integration probe uses locally imported assets and dummy SDL devices.
# It is deliberately outside CTest: normal builds must not require game data.
add_executable(original_boot_test src/main.c)
set_target_properties(original_boot_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_compile_definitions(original_boot_test PRIVATE VANILLABTN_BOOT_TEST)
target_compile_options(original_boot_test PRIVATE /W4)
target_link_libraries(original_boot_test PRIVATE game_main host_display SDL3::SDL3
    ps1_input ps1_audio ps1_card)

add_executable(original_farm_memory_test tests/original_farm_memory_test.c)
set_target_properties(original_farm_memory_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_compile_options(original_farm_memory_test PRIVATE /W4)
target_link_libraries(original_farm_memory_test PRIVATE game_main)
add_test(NAME original_farm_event_memory COMMAND original_farm_memory_test)

# The real runtime uses main.c's allocator. Isolated graphics fixtures provide
# their own bounded arena without exporting a second allocator into game_main.
add_library(sdk_test_work STATIC src/sdk_work.c)
target_link_libraries(sdk_test_work PUBLIC game_prim)
target_compile_options(sdk_test_work PRIVATE /W4)
foreach(fixture original_prim_test original_anim_test original_frame_test original_window_test)
    target_link_libraries(${fixture} PRIVATE sdk_test_work)
endforeach()
