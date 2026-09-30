if(NOT DEFINED GUARD_SCRIPT OR NOT DEFINED WORK_ROOT)
  message(FATAL_ERROR "GUARD_SCRIPT and WORK_ROOT are required.")
endif()
file(MAKE_DIRECTORY "${WORK_ROOT}/include/glyphweld")
file(WRITE "${WORK_ROOT}/include/glyphweld/forbidden_fixture.h" "#include <sys/socket.h>\n")
execute_process(
  COMMAND ${CMAKE_COMMAND} -DGW_RUNTIME_ROOT=${WORK_ROOT} -P ${GUARD_SCRIPT}
  RESULT_VARIABLE guard_result
  OUTPUT_VARIABLE guard_output
  ERROR_VARIABLE guard_error)
if(guard_result EQUAL 0 OR NOT "${guard_output}${guard_error}" MATCHES "platform header")
  message(FATAL_ERROR "Portable runtime guard accepted known forbidden header.")
endif()
message(STATUS "Portable runtime guard rejection fixture passed.")
