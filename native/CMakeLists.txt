cmake_minimum_required(VERSION 3.24)
project(VanillaBTNNative LANGUAGES C)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The native bringup currently requires an x64 MSVC toolchain on Windows")
endif()

include(FetchContent)
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

add_executable(vanillabtn WIN32 src/main.c)
set_target_properties(vanillabtn PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(vanillabtn PRIVATE SDL3::SDL3 ps1_video)
target_compile_options(vanillabtn PRIVATE /W4)

enable_testing()
add_test(NAME native_startup COMMAND vanillabtn --check-build)
add_executable(ps1_video_test tests/ps1_video_test.c)
set_target_properties(ps1_video_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_link_libraries(ps1_video_test PRIVATE ps1_video)
target_compile_options(ps1_video_test PRIVATE /W4)
add_test(NAME ps1_video_pixels COMMAND ps1_video_test)
