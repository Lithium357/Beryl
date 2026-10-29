# Optional real-compiler checks. Missing explicitly requested tools fail configure;
# no download, license activation, or compiler substitution happens during tests.
option(BERYL_COMPILER_MATRIX "Test additional GnuCOBOL dialects and configured compilers" OFF)
set(BERYL_GCOBOL_EXECUTABLE "" CACHE FILEPATH "GCC COBOL compiler (gcobol)")
set(BERYL_GCOBOL_FLAGS "" CACHE STRING "Additional GCC COBOL flags, e.g. toolchain search paths")
set(BERYL_OPENCOBOL_EXECUTABLE "" CACHE FILEPATH "opensource COBOL compiler (cobc)")
set(BERYL_GNUCOBOL31_EXECUTABLE "" CACHE FILEPATH "GnuCOBOL 3.1.2 compiler (cobc)")
set(BERYL_COBOL4J_EXECUTABLE "" CACHE FILEPATH "opensource COBOL 4J compiler (cobj)")
set(BERYL_JAVA_EXECUTABLE "" CACHE FILEPATH "Java runtime for COBOL 4J")
set(BERYL_COBOL4J_RUNTIME "" CACHE FILEPATH "COBOL 4J libcobj.jar runtime")
set(BERYL_MATRIX_cobol4j_ENVIRONMENT "" CACHE STRING "Environment assignments for the COBOL 4J compiler")
set(BERYL_IBM_COBOL_EXECUTABLE "" CACHE FILEPATH "Licensed IBM COBOL for Linux compiler (cob2)")
set(BERYL_IBM_SCU_EXECUTABLE "" CACHE FILEPATH "IBM COBOL source conversion utility (scu)")
set(BERYL_MATRIX_ibm_linux_ENVIRONMENT "COBRTOPT=FS=QSAM" CACHE STRING "Environment for IBM Linux fixed raw-record fixtures")

function(beryl_native_profile name compiler)
  if(NOT EXISTS "${compiler}")
    message(FATAL_ERROR "${name}: compiler does not exist: ${compiler}")
  endif()
  set(directory "${CMAKE_CURRENT_BINARY_DIR}/compiler-matrix/${name}")
  set(BERYL_MATRIX_${name}_ENVIRONMENT "" CACHE STRING "Environment assignments for the ${name} compiler and runtime")
  set(profile_environment ${BERYL_MATRIX_${name}_ENVIRONMENT})
  file(MAKE_DIRECTORY "${directory}")
  set(fixtures accounts accounts_bug codec_vectors generated_records)
  if(name MATCHES "^gnu_cobol(85|2002|2014)$")
    # These two fixtures use standard statements. Numeric/driver fixtures use
    # vendor extensions (COMP-3 spelling and RETURN-CODE, among others).
    set(fixtures accounts accounts_bug)
  endif()
  foreach(fixture IN LISTS fixtures)
    add_custom_command(OUTPUT "${directory}/${fixture}"
      COMMAND "${CMAKE_COMMAND}" -E env ${profile_environment} "${compiler}" ${ARGN} -o "${directory}/${fixture}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/cobol/${fixture}.cob"
      DEPENDS "tests/cobol/${fixture}.cob" VERBATIM)
    add_custom_target(matrix_${name}_${fixture} ALL DEPENDS "${directory}/${fixture}")
  endforeach()
  add_test(NAME matrix_${name}_acceptance COMMAND test_acceptance $<TARGET_FILE:beryl>
    "${directory}/accounts" "${directory}/accounts_bug" "${CMAKE_CURRENT_SOURCE_DIR}")
  set_tests_properties(matrix_${name}_acceptance PROPERTIES TIMEOUT 90 LABELS "compiler-matrix;${name}" ENVIRONMENT "${profile_environment}")
  if(name MATCHES "^gnu_cobol(85|2002|2014)$")
    return()
  endif()
  add_test(NAME matrix_${name}_codec COMMAND test_records --cobol $<TARGET_FILE:beryl> "${directory}/codec_vectors")
  add_test(NAME matrix_${name}_generation COMMAND test_generate $<TARGET_FILE:beryl> "${directory}/generated_records")
  add_test(NAME matrix_${name}_driver COMMAND test_driver $<TARGET_FILE:beryl>
    "${compiler}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/cobol/clamp_subprogram.cob" ${ARGN})
  set_tests_properties(matrix_${name}_acceptance matrix_${name}_codec
    matrix_${name}_generation matrix_${name}_driver PROPERTIES TIMEOUT 90 LABELS "compiler-matrix;${name}" ENVIRONMENT "${profile_environment}")
endfunction()

if(BERYL_COMPILER_MATRIX)
  if(COBC_EXECUTABLE)
    foreach(dialect ibm mf cobol85 cobol2002 cobol2014)
      beryl_native_profile(gnu_${dialect} "${COBC_EXECUTABLE}" -x -free "-std=${dialect}")
    endforeach()
  endif()
  if(BERYL_GCOBOL_EXECUTABLE)
    separate_arguments(gcobol_flags UNIX_COMMAND "${BERYL_GCOBOL_FLAGS}")
    beryl_native_profile(gcobol "${BERYL_GCOBOL_EXECUTABLE}" -ffree-form ${gcobol_flags})
  endif()
  if(BERYL_OPENCOBOL_EXECUTABLE)
    beryl_native_profile(opensource_cobol "${BERYL_OPENCOBOL_EXECUTABLE}" -x -free)
  endif()
  if(BERYL_GNUCOBOL31_EXECUTABLE)
    beryl_native_profile(gnu_312 "${BERYL_GNUCOBOL31_EXECUTABLE}" -x -free)
  endif()
  if(BERYL_COBOL4J_EXECUTABLE)
    foreach(required BERYL_COBOL4J_EXECUTABLE BERYL_JAVA_EXECUTABLE BERYL_COBOL4J_RUNTIME)
      if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "COBOL 4J requires an existing ${required}")
      endif()
    endforeach()
    set(directory "${CMAKE_CURRENT_BINARY_DIR}/compiler-matrix/cobol4j")
    file(MAKE_DIRECTORY "${directory}")
    # COBOL 4J's default Java main discards GOBACK's RETURN-CODE. This
    # documented configuration emits System.exit with the COBOL status.
    set(status_config "${directory}/beryl-status.conf")
    file(WRITE "${status_config}" "include \"default.conf\"\nenable-program-status-register: yes\n")
    foreach(fixture accounts accounts_bug codec_vectors generated_records driver)
      string(REPLACE "_" "__" class "${fixture}")
      if(fixture STREQUAL "driver")
        set(class beryl__driver)
      else()
        add_custom_command(OUTPUT "${directory}/${class}.class"
          COMMAND "${CMAKE_COMMAND}" -E env ${BERYL_MATRIX_cobol4j_ENVIRONMENT}
            "${BERYL_COBOL4J_EXECUTABLE}" -free "-conf=${status_config}" -o "${directory}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/cobol/${fixture}.cob"
          WORKING_DIRECTORY "${directory}" DEPENDS "tests/cobol/${fixture}.cob" "${status_config}" VERBATIM)
        add_custom_target(matrix_cobol4j_${fixture}_class ALL DEPENDS "${directory}/${class}.class")
      endif()
      add_executable(matrix_cobol4j_${fixture} tests/c/java_fixture.c)
      if(BERYL_SANITIZERS)
        # Targets run under RLIMIT_AS; ASan reserves a much larger virtual
        # address space before main. Instrument Beryl/tests, not this exec stub.
        target_compile_options(matrix_cobol4j_${fixture} PRIVATE -fno-sanitize=all)
        target_link_options(matrix_cobol4j_${fixture} PRIVATE -fno-sanitize=all)
      endif()
      target_compile_definitions(matrix_cobol4j_${fixture} PRIVATE
        BERYL_JAVA_EXECUTABLE="${BERYL_JAVA_EXECUTABLE}"
        BERYL_JAVA_CLASSPATH="${directory}:${BERYL_COBOL4J_RUNTIME}"
        BERYL_JAVA_CLASS="${class}")
    endforeach()
    add_test(NAME matrix_cobol4j_acceptance COMMAND test_acceptance $<TARGET_FILE:beryl>
      $<TARGET_FILE:matrix_cobol4j_accounts> $<TARGET_FILE:matrix_cobol4j_accounts_bug> "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME matrix_cobol4j_codec COMMAND test_records --cobol $<TARGET_FILE:beryl> $<TARGET_FILE:matrix_cobol4j_codec_vectors>)
    add_test(NAME matrix_cobol4j_generation COMMAND test_generate $<TARGET_FILE:beryl> $<TARGET_FILE:matrix_cobol4j_generated_records>)
    add_test(NAME matrix_cobol4j_driver COMMAND "${CMAKE_COMMAND}"
      "-DBERYL=$<TARGET_FILE:beryl>" "-DTEST_DRIVER=$<TARGET_FILE:test_driver>"
      "-DLAUNCHER=$<TARGET_FILE:matrix_cobol4j_driver>" "-DWORK=${directory}"
      "-DCOMPILER=${BERYL_COBOL4J_EXECUTABLE}" "-DRUNTIME=${BERYL_COBOL4J_RUNTIME}"
      "-DSTATUS_CONFIG=${status_config}"
      "-DSUBPROGRAM=${CMAKE_CURRENT_SOURCE_DIR}/tests/cobol/clamp_subprogram.cob"
      "-DPROFILE_ENVIRONMENT=${BERYL_MATRIX_cobol4j_ENVIRONMENT}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/JavaDriverTest.cmake")
    set_tests_properties(matrix_cobol4j_acceptance matrix_cobol4j_codec matrix_cobol4j_generation
      matrix_cobol4j_driver PROPERTIES TIMEOUT 90 LABELS "compiler-matrix;cobol4j")
  endif()
  if(BERYL_IBM_COBOL_EXECUTABLE)
    foreach(required BERYL_IBM_COBOL_EXECUTABLE BERYL_IBM_SCU_EXECUTABLE)
      if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "IBM Linux profile requires an existing ${required}")
      endif()
    endforeach()
    set(directory "${CMAKE_CURRENT_BINARY_DIR}/compiler-matrix/ibm_linux")
    file(MAKE_DIRECTORY "${directory}")
    foreach(fixture accounts accounts_bug codec_vectors generated_records)
      add_custom_command(OUTPUT "${directory}/${fixture}"
        COMMAND "${CMAKE_COMMAND}" "-DCOMPILER=${BERYL_IBM_COBOL_EXECUTABLE}"
          "-DSCU=${BERYL_IBM_SCU_EXECUTABLE}" "-DOUTPUT=${directory}/${fixture}"
          "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/cobol/${fixture}.cob"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompileIBM.cmake"
        DEPENDS "tests/cobol/${fixture}.cob" cmake/CompileIBM.cmake VERBATIM)
      add_custom_target(matrix_ibm_linux_${fixture} ALL DEPENDS "${directory}/${fixture}")
    endforeach()
    add_test(NAME matrix_ibm_linux_acceptance COMMAND test_acceptance $<TARGET_FILE:beryl>
      "${directory}/accounts" "${directory}/accounts_bug" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME matrix_ibm_linux_codec COMMAND test_records --cobol $<TARGET_FILE:beryl> "${directory}/codec_vectors" c)
    add_test(NAME matrix_ibm_linux_generation COMMAND test_generate $<TARGET_FILE:beryl> "${directory}/generated_records")
    add_test(NAME matrix_ibm_linux_driver COMMAND "${CMAKE_COMMAND}"
      "-DBERYL=$<TARGET_FILE:beryl>" "-DTEST_DRIVER=$<TARGET_FILE:test_driver>"
      "-DCOMPILER=${BERYL_IBM_COBOL_EXECUTABLE}" "-DSCU=${BERYL_IBM_SCU_EXECUTABLE}"
      "-DSUBPROGRAM=${CMAKE_CURRENT_SOURCE_DIR}/tests/cobol/clamp_subprogram.cob"
      "-DWORK=${directory}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/IBMDriverTest.cmake")
    set_tests_properties(matrix_ibm_linux_acceptance matrix_ibm_linux_codec
      matrix_ibm_linux_generation matrix_ibm_linux_driver PROPERTIES TIMEOUT 90
      LABELS "compiler-matrix;ibm_linux" ENVIRONMENT "${BERYL_MATRIX_ibm_linux_ENVIRONMENT}")
  endif()
endif()
