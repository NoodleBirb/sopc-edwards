add_subdirectory(unit)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME SOP_D4_runtime_oracle
         COMMAND ${Python3_EXECUTABLE} -m unittest discover
                 -s ${CMAKE_CURRENT_SOURCE_DIR}/system
                 -p test_sop_d4_runner.py)
add_test(NAME SOP_D4_manifest_consistency
         COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/system/sop_d4.py --check)
