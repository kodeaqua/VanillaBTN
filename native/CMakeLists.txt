set(vanillabtn_llvm OFF)
if(CMAKE_C_COMPILER_ID STREQUAL "Clang"
   AND CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "GNU"
   AND CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
    set(vanillabtn_llvm ON)
endif()
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8
   OR (NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC" AND NOT vanillabtn_llvm))
    message(FATAL_ERROR "Use MSVC or clang from a Visual Studio x64 developer shell.")
endif()
if(CMAKE_CONFIGURATION_TYPES OR NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release)$")
    message(FATAL_ERROR "Select the Windows x64 Debug or Release preset.")
endif()

if(vanillabtn_llvm)
    # LLVM's AddressSanitizer requires the release CRT even in Debug.
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
endif()
add_executable(vanillabtn main.c)
configure_file("${PROJECT_SOURCE_DIR}/tools/psx_disc.py" tools/psx_disc.py COPYONLY)
configure_file("${PROJECT_SOURCE_DIR}/disc/disc.sha1" disc/disc.sha1 COPYONLY)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(vanillabtn PRIVATE VANILLABTN_DEBUG)
    include(CheckCCompilerFlag)
    if(vanillabtn_llvm)
        target_compile_options(vanillabtn PRIVATE -O0 -g -Wall -Wextra)
        execute_process(COMMAND "${CMAKE_C_COMPILER}" --print-runtime-dir
            OUTPUT_VARIABLE runtime_dir OUTPUT_STRIP_TRAILING_WHITESPACE)
        set(asan_runtime "${runtime_dir}/clang_rt.asan_dynamic-x86_64.dll")
        set(sanitizers undefined)
        if(EXISTS "${asan_runtime}")
            list(APPEND sanitizers address)
        endif()
        foreach(sanitizer IN LISTS sanitizers)
            set(flag "-fsanitize=${sanitizer}")
            string(TOUPPER "${sanitizer}" sanitizer_name)
            set(supported "VANILLABTN_HAS_${sanitizer_name}_SANITIZER")
            set(CMAKE_REQUIRED_LINK_OPTIONS "${flag}")
            check_c_compiler_flag("${flag}" "${supported}")
            if(${supported})
                target_compile_options(vanillabtn PRIVATE "${flag}")
                target_link_options(vanillabtn PRIVATE "${flag}")
            endif()
        endforeach()
        unset(CMAKE_REQUIRED_LINK_OPTIONS)
        if(VANILLABTN_HAS_ADDRESS_SANITIZER AND EXISTS "${asan_runtime}")
            # Resolve LLVM's ASan DLL before any MSVC runtime with the same name on PATH.
            add_custom_command(TARGET vanillabtn POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${asan_runtime}" "$<TARGET_FILE_DIR:vanillabtn>"
                VERBATIM)
        endif()
    else()
        target_compile_options(vanillabtn PRIVATE /W4)
        check_c_compiler_flag(/fsanitize=address VANILLABTN_HAS_ASAN)
        if(VANILLABTN_HAS_ASAN)
            target_compile_options(vanillabtn PRIVATE /fsanitize=address)
            target_link_options(vanillabtn PRIVATE /INCREMENTAL:NO)
        endif()
    endif()
else()
    if(vanillabtn_llvm)
        target_compile_options(vanillabtn PRIVATE -O2)
    endif()
    set_property(TARGET vanillabtn PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

    install(TARGETS vanillabtn RUNTIME DESTINATION .)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tools/psx_disc.py" DESTINATION tools)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/disc/disc.sha1" DESTINATION disc)
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/NOTICE" DESTINATION .)
endif()

add_test(NAME vanillabtn_native_smoke
    COMMAND ${CMAKE_COMMAND} "-DEXECUTABLE=$<TARGET_FILE:vanillabtn>" "-DBUILD_CONFIG=$<CONFIG>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/smoke.cmake"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
