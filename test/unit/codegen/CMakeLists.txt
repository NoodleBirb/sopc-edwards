add_executable(codegen_unit_tests)
target_sources(codegen_unit_tests
               PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/CodegenFunctionsTest.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/CodegenStateIsolationTest.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/SOPD4CodegenTest.cpp)
target_include_directories(
  codegen_unit_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/src/codegen/
          ${CMAKE_SOURCE_DIR}/src/optimizer/)
target_link_libraries(
  codegen_unit_tests
  PRIVATE optimizer
          antlr4_static
          ${llvm_libs}
          ast
          error
          frontend
          semantic
          topc_codegen
          test_helpers
          topc_test_includes
          coverage_config
          Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(codegen_unit_tests)
