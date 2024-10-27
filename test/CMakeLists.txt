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
