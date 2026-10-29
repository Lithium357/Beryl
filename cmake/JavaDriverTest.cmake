string(RANDOM LENGTH 12 ALPHABET abcdef0123456789 suffix)
set(directory "${WORK}/driver-${suffix}")
file(MAKE_DIRECTORY "${directory}")
execute_process(COMMAND "${BERYL}" driver --program CLAMPREC --record-bytes 3
  --out "${directory}/source" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "COBOL 4J driver generation failed: ${result}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${PROFILE_ENVIRONMENT}
  "${COMPILER}" -free "-conf=${STATUS_CONFIG}" -o "${directory}" "${directory}/source/driver.cob" "${SUBPROGRAM}"
  WORKING_DIRECTORY "${directory}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "COBOL 4J driver compilation failed: ${result}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "BERYL_TEST_CLASSPATH=${directory}:${RUNTIME}"
  "${TEST_DRIVER}" "${BERYL}" --prebuilt "${LAUNCHER}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "COBOL 4J driver execution failed: ${result}")
endif()
