# Integration checks for D5: the public parser route must stop before analysis
# and code generation, report errors, and consume all input.
file(MAKE_DIRECTORY "${WORK_DIR}")
set(valid "${WORK_DIR}/sop.sop")
file(WRITE "${valid}" "main() { s = [1, 2]; for (e : &s) { f = (x) => x + e; } return #s; }")
execute_process(COMMAND "${TOPC}" --parse-only "${valid}"
                RESULT_VARIABLE status OUTPUT_VARIABLE tree ERROR_VARIABLE error)
if(NOT status EQUAL 0 OR NOT tree MATCHES "\\(program " OR NOT tree MATCHES "<EOF>")
  message(FATAL_ERROR "Valid SOP did not produce a complete parse tree: ${status}: ${error}: ${tree}")
endif()
if(EXISTS "${valid}.bc" OR EXISTS "${valid}.ll")
  message(FATAL_ERROR "--parse-only emitted compiler output")
endif()

foreach(fragment IN ITEMS
    "main() { x = s[1:2:3]; return 0; }"
    "main() { return 0; } x = 1;"
    "main() { return 0; } @")
  file(WRITE "${WORK_DIR}/invalid.sop" "${fragment}")
  execute_process(COMMAND "${TOPC}" --parse-only "${WORK_DIR}/invalid.sop"
                  RESULT_VARIABLE status OUTPUT_VARIABLE tree ERROR_VARIABLE error)
  if(NOT status EQUAL 1 OR NOT error MATCHES "parse error" OR NOT tree STREQUAL "")
    message(FATAL_ERROR "Invalid input was not diagnosed as a parse error: ${status}: ${error}: ${tree}")
  endif()
endforeach()

execute_process(COMMAND "${TOPC}" "${valid}"
                RESULT_VARIABLE status ERROR_VARIABLE error OUTPUT_VARIABLE output)
if(NOT status EQUAL 1 OR NOT error MATCHES "use --parse-only" OR error MATCHES "parse error")
  message(FATAL_ERROR "Normal AST path did not diagnose its SOP limitation: ${status}: ${error}")
endif()

execute_process(COMMAND "${TOPC}" --parse-only -o "${WORK_DIR}/unexpected.bc" "${valid}"
                RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 1 OR NOT error MATCHES "cannot be combined" OR EXISTS "${WORK_DIR}/unexpected.bc")
  message(FATAL_ERROR "Conflicting compilation option was not rejected: ${status}: ${error}")
endif()

execute_process(COMMAND "${TOPC}" --parse-only --past "${valid}"
                RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 1 OR NOT error MATCHES "cannot be combined")
  message(FATAL_ERROR "Conflicting AST option was not rejected: ${status}: ${error}")
endif()
