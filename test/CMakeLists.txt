# CMakeLists.txt in /test

# Fetch Catch2 only in the test directory to keep it separate from the main build
include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(Catch2)

# Find all test source files
file(GLOB_RECURSE test_src_files "*.cpp")

# Add test executable
add_executable(tests ${test_src_files})

# Include project headers
target_include_directories(tests PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Link CorGIReg, Catch2, and fmt to the test executable
target_link_libraries(tests PRIVATE CorGIReg Catch2::Catch2WithMain fmt::fmt)

# Enable testing
include(CTest)
include(Catch)

# Discover tests using Catch2
catch_discover_tests(tests
    EXTRA_ARGS "--durations=yes"
)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME Installer_Python COMMAND ${Python3_EXECUTABLE} -m unittest discover
        -s ${PROJECT_SOURCE_DIR}/test/installation -p "test_*.py" -v)
    add_test(NAME CLI_Python COMMAND ${Python3_EXECUTABLE} -m unittest discover
        -s ${PROJECT_SOURCE_DIR}/test/cli_python -p test_cli.py -v)
    set_tests_properties(CLI_Python PROPERTIES
        ENVIRONMENT "CORGIREG_ENGINE=$<TARGET_FILE:corgireg-match>")
endif()

set(CORGIREG_IMPORT_PYTHON "" CACHE FILEPATH "Python environment with optional ONNX and StableHLO readers for integration tests")
if(CORGIREG_IMPORT_PYTHON)
    add_test(NAME CLI_IR_Imports COMMAND ${CORGIREG_IMPORT_PYTHON} -m unittest discover
        -s ${PROJECT_SOURCE_DIR}/test/cli_python -p "test_import_*.py" -v)
    set_tests_properties(CLI_IR_Imports PROPERTIES
        ENVIRONMENT "CORGIREG_ENGINE=$<TARGET_FILE:corgireg-match>")
endif()
