# Using CorGIReg from a separate graph engine

CorGIReg provides matching, graph access, port bindings and graph generation.
Recipes, replacement policies, graph mutation and semantic verification belong
to the consuming project. No rewrite engine is built into the matching runtime.

## Install the C++ SDK

Use CMake 3.18 or newer and a C++17 compiler. Build with the same toolchain and
configuration that the consumer will use:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCORGIREG_BUILD_TESTS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake --install build --prefix /absolute/path/to/corgireg-sdk
```

On Windows, a prefix such as `C:/sdk/CorGIReg` is supported. For multi-configuration
generators, also pass `--config Debug` to build/install and `-C Debug` to CTest.
Do not install only the `Runtime` component when you need the C++ SDK: a full
installation includes the headers, static library, CMake metadata and bundled
fmt dependency. The existing Python installation script installs the runtime
commands; it is distinct from a full SDK installation.

The installed package supports:

```cmake
find_package(CorGIReg 0.1 CONFIG REQUIRED)
target_link_libraries(my_graph_engine PRIVATE CorGIReg::CorGIReg)
```

Configure the consumer with `-DCMAKE_PREFIX_PATH=/absolute/path/to/corgireg-sdk`.
The target propagates C++17, includes, fmt and Threads dependencies. It contains
no source-checkout include paths. Set `CORGIREG_BUILD_TESTS=OFF` when building
only the library and commands; Catch2 is then not fetched.

To create a binary ZIP containing the installed SDK and native commands:

```sh
cpack --config build/CPackConfig.cmake -G ZIP -B build/packages
```

This is a toolchain-specific C++ package, not a universal binary or a Python wheel.
Normal compiler runtime requirements still apply. The Python CLI and optional
ONNX/StableHLO readers are installed separately using `pyproject.toml` or the
[installation scripts](../installation/README.md).

## Public building blocks

| API | Consumer use |
| --- | --- |
| `GraphView` | Read-only adjacency and optional `edgePorts(from, to)` |
| `ImportedGraph` | Build an explicit graph with source node IDs and port bindings |
| `BasicGraphAdapter` | Expose a generated `GraphNN` through the same graph interface |
| `PGInterpreterTG` / `TGCompiled::test` | Compile a pattern and retrieve source captures |
| `IRGenerationOptions` / `PGInterpreterIR` | Instantiate a replacement fragment with explicit repetitions |
| `writeGraphJson` | Export generated topology with node IDs and port connections |
| `NodeLibrary` / `addDefinitionPredicates` | Load node-definition directories and turn them into predicates |

An edge port binding stores the destination input and source output separately.
Either can be unspecified. IDs identify nodes; ports identify uses of their values.
An adapter without port metadata remains usable for unnumbered patterns, but
cannot satisfy a fixed numbered arrow. See [graph JSON](graph-json.md) and
[generation](ir-generation.md).

## External transformation integration test

[examples/consumer](../examples/consumer/CMakeLists.txt) is a standalone CMake
project. It uses only `find_package` and the exported target. Its executable:

1. Builds a graph containing `Sub(x,y)`, two output consumers and an unrelated branch.
2. Uses matching to recover the original subtraction node's ID.
3. Generates a `Neg-2->Add#` replacement fragment.
4. Reconstructs a graph on the consumer side, mapping x to Add input 1 and y to
   Neg input 1, while preserving the original producers' output numbers.
5. Reconnects both external output consumers, preserving their input numbers.
6. Checks untouched IDs/names/edges, the original graph's immutability, 48 output
   evaluations and rejection of an ambiguous input mapping.

The example is one handwritten scalar recipe, not a generic rewrite engine or
a proof of floating-point equivalence. Operator arity, legality, overlaps and
rewrite scheduling remain the future engine's responsibilities.

The [JSON recipe prototype](../examples/rewrite/README.md) adds a data-driven
consumer alongside this C++ example. Recipes describe source/target patterns,
capture-derived repetition counts and port mappings. Its tests transform a
multi-node subgraph, preserve outside connections, reject ambiguous mappings and
match the written output again. The installed-package test also runs those
recipes using the relocated native commands when Python is available.

Run the installed-consumer validation with:

```sh
ctest --test-dir build -R Installed_Package_Rewrite_Integration --output-on-failure
```

The test installs into a fresh build-local prefix, relocates that SDK, configures
and links the external example, and executes its checks. This validates the
installed dependency boundary as well as the example transformation.

To try it manually after installation:

```sh
cmake -S examples/consumer -B build/consumer -DCMAKE_PREFIX_PATH=/absolute/path/to/corgireg-sdk
cmake --build build/consumer
ctest --test-dir build/consumer --output-on-failure
```
