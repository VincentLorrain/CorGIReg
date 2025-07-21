# AGENTS.md


## After modifying the code
1. Configure and build in Debug mode:
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
2. Run tests:
   cd build && ctest
# All tests must pass before committing.

