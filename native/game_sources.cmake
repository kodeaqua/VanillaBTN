if(NOT EXISTS "${VANILLABTN_PSYZ_SOURCE_DIR}/psyz/CMakeLists.txt"
   OR NOT EXISTS "${VANILLABTN_PSYZ_SOURCE_DIR}/external/SDL/CMakeLists.txt")
    message(FATAL_ERROR "Point VANILLABTN_PSYZ_SOURCE_DIR to a PsyZ checkout with SDL initialized.")
endif()

enable_language(CXX)
add_subdirectory("${VANILLABTN_PSYZ_SOURCE_DIR}/psyz" psyz EXCLUDE_FROM_ALL)

# Reuse the PS1 manifests for source selection while linking the game as one host executable.
function(psx_exe name)
    cmake_parse_arguments(PARSE_ARGV 1 image ""
        "BASE;STACK;REGION_TEXT;GP;SDK_OBJECTS;SDK_PATCHES" "SOURCES;SMALL_COMMONS")
    list(TRANSFORM image_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
    target_sources(vanillabtn PRIVATE ${image_SOURCES})
endfunction()

function(psx_overlay name)
    cmake_parse_arguments(PARSE_ARGV 1 image "OWN_COMMONS" "FILE;REGION" "SOURCES;GAP")
    list(TRANSFORM image_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
    target_sources(vanillabtn PRIVATE ${image_SOURCES})
endfunction()

foreach(image IN LISTS VANILLABTN_IMAGE_DIRS)
    add_subdirectory("${PROJECT_SOURCE_DIR}/${image}" "${image}")
endforeach()

list(TRANSFORM VANILLABTN_INCLUDE_DIRS PREPEND "${PROJECT_SOURCE_DIR}/")
target_include_directories(vanillabtn PRIVATE ${VANILLABTN_INCLUDE_DIRS})
target_compile_definitions(vanillabtn PRIVATE VANILLABTN_HAS_GAME)
target_compile_options(vanillabtn PRIVATE -funsigned-char)
target_sources(vanillabtn PRIVATE sdk_compat.c)
target_link_libraries(vanillabtn PRIVATE psyz)
set_source_files_properties("${PROJECT_SOURCE_DIR}/src/main/main.c"
    PROPERTIES COMPILE_DEFINITIONS "main=vanillabtn_game_main")
