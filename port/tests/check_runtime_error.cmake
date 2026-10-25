execute_process(COMMAND "${PROGRAM}" "${CASE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 10)
if("${result}" STREQUAL "0" OR "${result}" MATCHES "timeout")
    message(FATAL_ERROR "${CASE} did not fail promptly: ${result}")
endif()
string(FIND "${error}" "PSX runtime error: ${CONTEXT}: ${DETAIL}" diagnostic)
if(diagnostic EQUAL -1)
    message(FATAL_ERROR "${CASE} failed without the expected diagnostic: ${result}: ${error}")
endif()
