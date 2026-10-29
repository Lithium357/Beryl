# Optional real-compiler checks. Missing explicitly requested tools fail configure;
# no download, license activation, or compiler substitution happens during tests.
option(BERYL_COMPILER_MATRIX "Test additional GnuCOBOL dialects and configured compilers" OFF)
set(BERYL_GCOBOL_EXECUTABLE "" CACHE FILEPATH "GCC COBOL compiler (gcobol)")
set(BERYL_GCOBOL_FLAGS "" CACHE STRING "Additional GCC COBOL flags, e.g. toolchain search paths")
set(BERYL_OPENCOBOL_EXECUTABLE "" CACHE FILEPATH "opensource COBOL compiler (cobc)")
set(BERYL_GNUCOBOL31_EXECUTABLE "" CACHE FILEPATH "GnuCOBOL 3.1.2 compiler (cobc)")

function(beryl_native_profile name compiler)
  if(NOT EXISTS "${compiler}")
    message(FATAL_ERROR "${name}: compiler does not exist: ${compiler}")
  endif()
  set(directory "${CMAKE_CURRENT_BINARY_DIR}/compiler-matrix/${name}")
  file(MAKE_DIRECTORY "${directory}")
  set(fixtures accounts accounts_bug codec_vectors generated_records)
  if(name MATCHES "^gnu_cobol(85|2002|2014)$")
    # These two fixtures use standard statements. Numeric/driver fixtures use
    # vendor extensions (COMP-3 spelling and RETURN-CODE, among others).
    set(fixtures accounts accounts_bug)
  endif()
  foreach(fixture IN LISTS fixtures)
    add_custom_command(OUTPUT "${directory}/${fixture}"
      COMMAND "${compiler}" ${ARGN} -o "${directory}/${fixture}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/cobol/${fixture}.cob"
      DEPENDS "tests/cobol/${fixture}.cob" VERBATIM)
    add_custom_target(matrix_${name}_${fixture} ALL DEPENDS "${directory}/${fixture}")
  endforeach()
  add_test(NAME matrix_${name}_acceptance COMMAND test_acceptance $<TARGET_FILE:beryl>
    "${directory}/accounts" "${directory}/accounts_bug" "${CMAKE_CURRENT_SOURCE_DIR}")
  set_tests_properties(matrix_${name}_acceptance PROPERTIES TIMEOUT 90 LABELS "compiler-matrix;${name}")
  if(name MATCHES "^gnu_cobol(85|2002|2014)$")
    return()
  endif()
  add_test(NAME matrix_${name}_codec COMMAND test_records --cobol $<TARGET_FILE:beryl> "${directory}/codec_vectors")
  add_test(NAME matrix_${name}_generation COMMAND test_generate $<TARGET_FILE:beryl> "${directory}/generated_records")
  add_test(NAME matrix_${name}_driver COMMAND test_driver $<TARGET_FILE:beryl>
    "${compiler}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/cobol/clamp_subprogram.cob" ${ARGN})
  set_tests_properties(matrix_${name}_acceptance matrix_${name}_codec
    matrix_${name}_generation matrix_${name}_driver PROPERTIES TIMEOUT 90 LABELS "compiler-matrix;${name}")
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
endif()
