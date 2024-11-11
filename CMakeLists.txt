cmake_minimum_required(VERSION 3.12)
project(CorGIReg VERSION 0.0)


# if(WIN32)
#   set(CMAKE_EXE_LINKER_FLAGS "-Wl,-subsystem,console")
# elseif(MSVC)
#   # For Visual Studio Compiler
#   set_target_properties(your_target PROPERTIES
#       LINK_FLAGS "/SUBSYSTEM:CONSOLE"
#   )
# else()
#   # For GCC or Clang on Unix-like systems
#   set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,console")
# endif()


# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add external dependencies
Include(FetchContent)

# Fetch fmt library (for both debug and release)
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 11.0.2
)
FetchContent_MakeAvailable(fmt)

# Add source files for the main application
#file(GLOB_RECURSE src_files "src/*.cpp")


set(src_files
    ${PROJECT_SOURCE_DIR}/src/BasicGraphIr/Graph.cpp
    ${PROJECT_SOURCE_DIR}/src/CompileTool/CompileTransitionGraph/PGInterpreterIR.cpp
    ${PROJECT_SOURCE_DIR}/src/CompileTool/CompileTransitionGraph/PGInterpreterTG.cpp
    ${PROJECT_SOURCE_DIR}/src/CompileTool/CompileTransitionGraph/PartitionGraphLexer.cpp
    ${PROJECT_SOURCE_DIR}/src/CompileTool/CompileTransitionGraph/PartitionGraphParser.cpp
    ${PROJECT_SOURCE_DIR}/src/NodeTest/Expression.cpp
    ${PROJECT_SOURCE_DIR}/src/NodeTest/ExpressionParser.cpp
    ${PROJECT_SOURCE_DIR}/src/NodeTest/FunctionBuilder.cpp
    ${PROJECT_SOURCE_DIR}/src/NodeTest/NodeCondition.cpp
    ${PROJECT_SOURCE_DIR}/src/NodeTest/NodeDictionary.cpp
    ${PROJECT_SOURCE_DIR}/src/NodeTest/PredefinedNodeCondition.cpp
    ${PROJECT_SOURCE_DIR}/src/TransitionGraph/MatchSolution.cpp
    ${PROJECT_SOURCE_DIR}/src/TransitionGraph/RunTimeContext.cpp
    ${PROJECT_SOURCE_DIR}/src/TransitionGraph/State.cpp
    ${PROJECT_SOURCE_DIR}/src/TransitionGraph/Transition.cpp
    ${PROJECT_SOURCE_DIR}/src/TransitionGraph/TransitionGraph.cpp
)

# Create a static library for CorGIReg to share across executables
add_library(CorGIReg STATIC ${src_files})

# Specify include directories for the library, include the hierarchical "include" directory
target_include_directories(CorGIReg
    PRIVATE ${PROJECT_SOURCE_DIR}/include  # Add the root of the include directory hierarchy
)

# Link fmt to the library
target_link_libraries(CorGIReg PRIVATE fmt::fmt)

# Optionally include tests only in Debug mode
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  enable_testing()
  add_subdirectory(test) # Include the test subdirectory
endif()

# Include the doc/CMakeLists.txt for Doxygen
# add_subdirectory(doc)
