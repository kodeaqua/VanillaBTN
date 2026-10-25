execute_process(COMMAND "${PROGRAM}" "${API}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 10)
if("${result}" STREQUAL "0")
    message(FATAL_ERROR "${API} silently succeeded")
endif()
if(NOT "${error}" MATCHES "Unimplemented PSX API: ${API}")
    message(FATAL_ERROR "${API} failed without the expected diagnostic: ${result}: ${error}")
endif()
