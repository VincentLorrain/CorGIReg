cmake_minimum_required(VERSION 3.18)
project(CorGIReg VERSION 0.1.0 LANGUAGES CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)



# if(MSVC)
#     # For Visual Studio Compiler
#     set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")
# elseif(WIN32)
#     # For MinGW or similar on Windows not using MSVC
#     set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,console")
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
    ${PROJECT_SOURCE_DIR}/src/Cli/SearchExecutor.cpp
    ${PROJECT_SOURCE_DIR}/src/Cli/MatchRequest.cpp
    ${PROJECT_SOURCE_DIR}/src/BasicGraphIr/Graph.cpp
    ${PROJECT_SOURCE_DIR}/src/CompileTool/CompileTransitionGraph/PGInterpreterIR.cpp
    ${PROJECT_SOURCE_DIR}/src/CompileTool/CompileTransitionGraph/PGInterpreterTG.cpp
    ${PROJECT_SOURCE_DIR}/src/CompileTool/PostAstOptimization/RootSelectionPass.cpp
    ${PROJECT_SOURCE_DIR}/src/CompileTool/CompileTransitionGraph/PartitionGraphLexer.cpp
    ${PROJECT_SOURCE_DIR}/src/CompileTool/CompileTransitionGraph/PartitionGraphParser.cpp
    ${PROJECT_SOURCE_DIR}/src/NodeTest/Expression.cpp
    ${PROJECT_SOURCE_DIR}/src/NodeTest/ExpressionParser.cpp
    ${PROJECT_SOURCE_DIR}/src/NodeTest/FunctionBuilder.cpp
    ${PROJECT_SOURCE_DIR}/src/NodeTest/NodeCondition.cpp
    ${PROJECT_SOURCE_DIR}/src/NodeTest/NodeRule.cpp
    ${PROJECT_SOURCE_DIR}/src/NodeTest/NodeDictionary.cpp
    ${PROJECT_SOURCE_DIR}/src/NodeTest/PredefinedNodeCondition.cpp
    ${PROJECT_SOURCE_DIR}/src/TransitionGraph/MatchSolution.cpp
    ${PROJECT_SOURCE_DIR}/src/TransitionGraph/RunTimeContext.cpp
    ${PROJECT_SOURCE_DIR}/src/TransitionGraph/State.cpp
    ${PROJECT_SOURCE_DIR}/src/TransitionGraph/Transition.cpp
    ${PROJECT_SOURCE_DIR}/src/TransitionGraph/TransitionGraph.cpp
    ${PROJECT_SOURCE_DIR}/src/Grexgex/RandomExpressionGenerator.cpp
)

# Create a static library for CorGIReg to share across executables
add_library(CorGIReg STATIC ${src_files})
add_library(CorGIReg::CorGIReg ALIAS CorGIReg)
target_compile_features(CorGIReg PUBLIC cxx_std_17)

# Specify include directories for the library, include the hierarchical "include" directory
target_include_directories(CorGIReg
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Link fmt to the library
find_package(Threads REQUIRED)
target_link_libraries(CorGIReg PUBLIC fmt::fmt Threads::Threads)

install(TARGETS CorGIReg EXPORT CorGIRegTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT Development)
install(EXPORT CorGIRegTargets NAMESPACE CorGIReg::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CorGIReg COMPONENT Development)
configure_package_config_file(cmake/CorGIRegConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CorGIRegConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CorGIReg)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/CorGIRegConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CorGIRegConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/CorGIRegConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CorGIReg COMPONENT Development)

add_executable(corgireg-match src/Cli/main.cpp)
target_include_directories(corgireg-match PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(corgireg-match PRIVATE CorGIReg)
install(TARGETS corgireg-match RUNTIME DESTINATION bin COMPONENT Runtime)
add_executable(corgireg-generate src/Cli/generate.cpp)
target_include_directories(corgireg-generate PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(corgireg-generate PRIVATE CorGIReg)
install(TARGETS corgireg-generate RUNTIME DESTINATION bin COMPONENT Runtime)

option(CORGIREG_BUILD_BENCHMARKS "Build reproducible native performance measurements" OFF)
if(CORGIREG_BUILD_BENCHMARKS)
  add_executable(corgireg-benchmark benchmarks/native.cpp)
  target_include_directories(corgireg-benchmark PRIVATE ${PROJECT_SOURCE_DIR}/include)
  target_link_libraries(corgireg-benchmark PRIVATE CorGIReg fmt::fmt)
  target_compile_definitions(corgireg-benchmark PRIVATE
    CORGIREG_BENCH_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    CORGIREG_BENCH_CONFIG="$<CONFIG>")
endif()

# Optionally include tests only in Debug mode


# Set compiler options
# if(MSVC)
#     set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od")
#     set(CMAKE_CXX_FLAGS_RELEASE "/O2")
# else()
#     set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
#     set(CMAKE_CXX_FLAGS_RELEASE "-O3")
# endif()

set(_corgireg_tests_default OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(_corgireg_tests_default ON)
endif()
option(CORGIREG_BUILD_TESTS "Build CorGIReg tests" ${_corgireg_tests_default})
if(CORGIREG_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test) # Include the test subdirectory
endif()

set(CPACK_PACKAGE_NAME CorGIReg)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR ZIP)
include(CPack)

# Include the doc/CMakeLists.txt for Doxygen
# add_subdirectory(doc)
