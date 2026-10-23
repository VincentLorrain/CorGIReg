# Importers and source identities

The CLI accepts JSON, ONNX and textual StableHLO. All importers produce the
same [operation graph](graph-json.md); the matcher operates on that graph through
[GraphView](graph-adapters.md). Import does not execute or optimize the network.

| Format | Automatic extensions | Dependency | ID convention |
| --- | --- | --- | --- |
| JSON | `.json` | Python standard library | Explicit unsigned 64-bit IDs |
| ONNX | `.onnx` | `onnx` extra | Zero-based position in `graph.node` |
| StableHLO | `.mlir`, `.stablehlo` | `jaxlib` via `stablehlo` extra | Operation index in the selected function block |

Use `-f` to override extension detection. Install both optional importers with
`python installation/install.py --formats all`; see [installation](../installation/README.md).
The supported StableHLO stack requires Python 3.12 or newer. Imports are lazy:
JSON matching does not require either optional package.

## ONNX

The importer reads the main graph. A default-domain operation uses its type,
such as `Conv`; a custom-domain operation uses `domain.Op`. A node's original
name is preserved, with `node_<index>` as fallback. Declared inputs, outputs,
domain and attributes are retained as node metadata.

Edges join each value's producer to its consumers. Graph inputs and initializers
are boundaries, not extra nodes. Missing optional inputs (`""`) create no edge.
Duplicate producers, undefined values and invalid graph outputs are rejected;
operation producers must precede their consumers in source order. Multiple
tensor connections between the same operations collapse to one adjacency edge.

Nested graph attributes (`GRAPH`/`GRAPHS`) are unsupported. Local function calls
remain opaque operations; function bodies are not expanded. External tensor data
is not loaded. This import validation does not replace the complete ONNX checker.

Declared scalar/list integer, float and string attributes become JSON values.
Defaults are not inferred. Tensor attributes expose `kind`, `shape`, `data_type`,
not weights. Non-UTF8 bytes use `bytes_hex`; nonfinite values use `nonfinite`;
attribute references use `reference`; unsupported types retain
`onnx_attribute_type`. A convolution with an absent `kernel_shape` therefore
does not satisfy a rule requiring that key merely because its weights imply 3x3.

## StableHLO / OpenXLA

The importer parses textual MLIR using the native MLIR bindings supplied by
jaxlib. It selects `main`, otherwise the sole function, or the function named
with `--function`. Ambiguous function selection requires that option.

The selected function must have one block. Its operation nodes must belong to
the `stablehlo` dialect and have no nested regions. `func.return` is a boundary,
not a matched node; arguments are also boundaries. Nested control flow, calls,
other operation dialects, raw HLO and bytecode are outside this importer subset.

Operation names retain the dialect, for example `stablehlo.convolution`. Display
names derive from the function and canonical SSA results, with an operation-index
fallback. Metadata includes `function`, `results`, `location`, input SSA names
and attributes. IDs are block-operation indices, not result-value indices; a
multi-result operation is still one node. Do not assume IDs remain identical
after rewriting or reserializing the source IR.

Builtin scalar/array attributes become JSON values. Dense numeric attributes up
to 4096 elements become nested arrays; larger ones retain shape/type metadata.
Other dialect attributes use `{"mlir": "textual representation"}`. Nonfinite
numbers use an explicit `nonfinite` object. Tensor operand types are not used
to synthesize attributes such as convolution kernel shape.

## Adding an importer

For a file format, implement a loader returning `nodes` and `edges`, preserve
source IDs/names and useful metadata, and apply the shared graph validation.
Register the format and extension in the CLI and add explicit positive and
unsupported-input tests. Add format mappings to semantic node definitions as
needed; a definition alone does not install an importer. OpenVX is not currently
an accepted CLI input format.

For an existing C++ graph, implement `GraphView` directly instead of serializing
to JSON. Parent and child adjacency must agree; predicates and names must refer
to the same node identities. See [adapter contracts](graph-adapters.md).
