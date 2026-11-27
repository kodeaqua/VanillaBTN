get_filename_component(executable_directory "${EXECUTABLE}" DIRECTORY)
set(log "${executable_directory}/VanillaBTN.log")

foreach(run RANGE 1 2)
    execute_process(COMMAND "${EXECUTABLE}" RESULT_VARIABLE result)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "VanillaBTN run ${run} failed (${result}).")
    endif()

    if(BUILD_CONFIG STREQUAL "Debug")
        file(STRINGS "${log}" entries)
        list(LENGTH entries count)
        set(timestamp "^[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+")
        if(NOT count EQUAL 1 OR NOT entries MATCHES "${timestamp} INFO startup VanillaBTN initialized$")
            message(FATAL_ERROR "Debug must rewrite a single timestamped startup line on each run.")
        endif()
    elseif(EXISTS "${log}")
        message(FATAL_ERROR "Release created a log file.")
    endif()
endforeach()

execute_process(COMMAND "${EXECUTABLE}" --import-disc RESULT_VARIABLE result
    ERROR_VARIABLE diagnostic)
if(NOT result STREQUAL "2" OR NOT diagnostic MATCHES "Usage: vanillabtn")
    message(FATAL_ERROR "The import option must report a missing disc argument.")
endif()

if(HAS_GAME)
    set(invalid_disc "${CMAKE_CURRENT_BINARY_DIR}/smoke_invalid_disc")
    file(WRITE "${invalid_disc}" "Invalid reference image")
    execute_process(COMMAND "${EXECUTABLE}" --disc "${invalid_disc}" RESULT_VARIABLE result
        ERROR_VARIABLE diagnostic TIMEOUT 15)
    file(REMOVE "${invalid_disc}")
    if(NOT result STREQUAL "1" OR NOT diagnostic MATCHES "does not match the USA reference SHA-1")
        message(FATAL_ERROR "A mismatched disc must be rejected before boot.")
    endif()
endif()

# A valid environment value can leave too little room for the application directory.
string(REPEAT "x" 255 long_directory)
set(ENV{LOCALAPPDATA} "${long_directory}")
execute_process(COMMAND "${EXECUTABLE}" --import-disc unused RESULT_VARIABLE result
    ERROR_VARIABLE diagnostic)
if(NOT result STREQUAL "1" OR NOT diagnostic MATCHES "Could not locate the local data directory")
    message(FATAL_ERROR "An oversized local data path must return an import error.")
endif()
