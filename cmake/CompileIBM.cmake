# Format a generated copy using IBM's own initial-formatting mode. -N avoids
# scu's statement/semantic rewrite passes; original sources remain untouched.
get_filename_component(directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${directory}")
execute_process(COMMAND "${SCU}" -f -N -E "${SOURCE}"
  OUTPUT_FILE "${OUTPUT}.source.cbl" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "IBM source formatting failed: ${result}")
endif()
set(extra_source)
if(SUBPROGRAM)
  set(extra_source "${OUTPUT}.subprogram.cbl")
  execute_process(COMMAND "${SCU}" -f -N -E "${SUBPROGRAM}"
    OUTPUT_FILE "${extra_source}" RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "IBM subprogram formatting failed: ${result}")
  endif()
endif()
execute_process(COMMAND "${COMPILER}" -q64 "-qsrcformat(extend)" -o "${OUTPUT}"
  "${OUTPUT}.source.cbl" ${extra_source} WORKING_DIRECTORY "${directory}"
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "IBM compilation failed: ${result}")
endif()
