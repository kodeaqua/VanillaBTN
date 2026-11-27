if(NOT WIN32 OR NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Use the Windows x64 presets from a Visual Studio x64 developer shell.")
endif()
if(CMAKE_CONFIGURATION_TYPES OR NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release)$")
    message(FATAL_ERROR "Select the Windows x64 Debug or Release preset.")
endif()

add_executable(vanillabtn main.c)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(vanillabtn PRIVATE VANILLABTN_DEBUG)
    target_compile_options(vanillabtn PRIVATE /W4)
    include(CheckCCompilerFlag)
    check_c_compiler_flag(/fsanitize=address VANILLABTN_HAS_ASAN)
    if(VANILLABTN_HAS_ASAN)
        target_compile_options(vanillabtn PRIVATE /fsanitize=address)
        target_link_options(vanillabtn PRIVATE /INCREMENTAL:NO)
    endif()
else()
    set_property(TARGET vanillabtn PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

add_test(NAME vanillabtn_native_smoke
    COMMAND ${CMAKE_COMMAND} "-DEXECUTABLE=$<TARGET_FILE:vanillabtn>" "-DBUILD_CONFIG=$<CONFIG>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/smoke.cmake"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
