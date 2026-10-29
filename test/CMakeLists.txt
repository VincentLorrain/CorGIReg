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
# Extended seeded generation-to-matching campaigns (label "extended"), off by
# default. The fast deterministic subset always runs with the Catch2 tests.
option(CORGIREG_EXTENDED_TESTS "Register extended randomized generation-matching campaigns" OFF)
if(CORGIREG_EXTENDED_TESTS)
    set(CORGIREG_GM_ITERATIONS 2000 CACHE STRING "Extended campaign: cases per test")
    set(CORGIREG_GM_SEED 1 CACHE STRING "Extended campaign: first seed")
    set(CORGIREG_GM_DEPTH 2 CACHE STRING "Extended campaign: maximum group nesting (0..6)")
    set(CORGIREG_GM_MAX_LOOPS 2 CACHE STRING "Extended campaign: maximum count of * and extra + iterations")
    set(CORGIREG_GM_MAX_NODES 12 CACHE STRING "Extended campaign: maximum generated nodes")
    set(CORGIREG_GM_EXTRA_NODES 6 CACHE STRING "Extended campaign: added nodes per enrichment step")
    set(CORGIREG_GM_EXTRA_EDGES 8 CACHE STRING "Extended campaign: added edges per enrichment step")
    set(CORGIREG_GM_MAX_STARTS 1500 CACHE STRING "Extended campaign: start tuples per search")
    set(CORGIREG_GM_SECONDS 300 CACHE STRING "Extended campaign: wall-clock budget per test case, checked between cases")
    add_test(NAME Generation_Matching_Extended COMMAND tests "[generation-matching][random]")
    set_tests_properties(Generation_Matching_Extended PROPERTIES LABELS "extended" TIMEOUT 3600
        ENVIRONMENT "CORGIREG_GM_ITERATIONS=${CORGIREG_GM_ITERATIONS};CORGIREG_GM_SEED=${CORGIREG_GM_SEED};CORGIREG_GM_DEPTH=${CORGIREG_GM_DEPTH};CORGIREG_GM_MAX_LOOPS=${CORGIREG_GM_MAX_LOOPS};CORGIREG_GM_MAX_NODES=${CORGIREG_GM_MAX_NODES};CORGIREG_GM_EXTRA_NODES=${CORGIREG_GM_EXTRA_NODES};CORGIREG_GM_EXTRA_EDGES=${CORGIREG_GM_EXTRA_EDGES};CORGIREG_GM_MAX_STARTS=${CORGIREG_GM_MAX_STARTS};CORGIREG_GM_SECONDS=${CORGIREG_GM_SECONDS};CORGIREG_GM_ARTIFACTS=${PROJECT_BINARY_DIR}/generation-matching-failures")
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
configure_file(${PROJECT_SOURCE_DIR}/cmake/PackageIntegration.cmake.in
    ${PROJECT_BINARY_DIR}/PackageIntegration.cmake @ONLY)
add_test(NAME Installed_Package_Rewrite_Integration
    COMMAND ${CMAKE_COMMAND} -DCONFIG=$<CONFIG> -P ${PROJECT_BINARY_DIR}/PackageIntegration.cmake)
set_tests_properties(Installed_Package_Rewrite_Integration PROPERTIES TIMEOUT 180 LABELS "integration;package")

if(Python3_Interpreter_FOUND)
    add_test(NAME Benchmark_Reports COMMAND ${Python3_EXECUTABLE} -m unittest discover
        -s ${PROJECT_SOURCE_DIR}/test/benchmarks -p "test_*.py" -v)
    add_test(NAME Installer_Python COMMAND ${Python3_EXECUTABLE} -m unittest discover
        -s ${PROJECT_SOURCE_DIR}/test/installation -p "test_*.py" -v)
    add_test(NAME CLI_Python COMMAND ${Python3_EXECUTABLE} -m unittest discover
        -s ${PROJECT_SOURCE_DIR}/test/cli_python -p test_cli.py -v)
    set_tests_properties(CLI_Python PROPERTIES
        ENVIRONMENT "CORGIREG_ENGINE=$<TARGET_FILE:corgireg-match>")
    add_test(NAME Generator_CLI COMMAND ${Python3_EXECUTABLE} -m unittest discover
        -s ${PROJECT_SOURCE_DIR}/test/cli_python -p test_generator.py -v)
    set_tests_properties(Generator_CLI PROPERTIES
        ENVIRONMENT "CORGIREG_GENERATOR=$<TARGET_FILE:corgireg-generate>;CORGIREG_ENGINE=$<TARGET_FILE:corgireg-match>")
    add_test(NAME Recipe_Rewrite_Integration COMMAND ${Python3_EXECUTABLE} -m unittest discover
        -s ${PROJECT_SOURCE_DIR}/examples/rewrite -p test_recipes.py -v)
    set_tests_properties(Recipe_Rewrite_Integration PROPERTIES LABELS "integration;recipe" TIMEOUT 120
        ENVIRONMENT "PYTHONPATH=${PROJECT_SOURCE_DIR}/python;CORGIREG_GENERATOR=$<TARGET_FILE:corgireg-generate>;CORGIREG_ENGINE=$<TARGET_FILE:corgireg-match>")
endif()

if(CORGIREG_BUILD_BENCHMARKS)
    add_test(NAME Benchmark_Smoke COMMAND corgireg-benchmark --samples 1 --iterations 1)
    set_tests_properties(Benchmark_Smoke PROPERTIES LABELS "benchmark" TIMEOUT 60)
endif()

set(CORGIREG_IMPORT_PYTHON "" CACHE FILEPATH "Python environment with optional ONNX and StableHLO readers for integration tests")
if(CORGIREG_IMPORT_PYTHON)
    add_test(NAME CLI_IR_Imports COMMAND ${CORGIREG_IMPORT_PYTHON} -m unittest discover
        -s ${PROJECT_SOURCE_DIR}/test/cli_python -p "test_import_*.py" -v)
    set_tests_properties(CLI_IR_Imports PROPERTIES
        ENVIRONMENT "CORGIREG_ENGINE=$<TARGET_FILE:corgireg-match>")
endif()
