add_subdirectory(unit)

add_test(NAME sop_parser_cli
  COMMAND ${CMAKE_COMMAND}
    -DTOPC=$<TARGET_FILE:topc>
    -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/sop-parser-cli
    -P ${CMAKE_CURRENT_SOURCE_DIR}/system/sop-parser-cli.cmake)
