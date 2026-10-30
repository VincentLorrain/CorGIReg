# Reusable node and pattern libraries

The pattern parser/automaton and the imported graph remain separate. A node
library supplies a shared vocabulary between them: `conv` can designate ONNX
`Conv` or StableHLO `stablehlo.convolution`, while captures retain the original
graph IDs and names. These mappings classify operations; they do not establish
numerical equivalence between IRs.

## Run the example

After the [CLI installation](cli.md), from the repository root:

```sh
corgireg -i examples/library/conv.mlir --node-library examples/library/nodes -R examples/library/patterns/conv-chain.txt -o build/conv-matches.json
corgireg -i model.onnx --node-library examples/library/nodes -R examples/library/patterns/conv-chain.txt -o build/onnx-conv-matches.json
```

The same pattern file contains `conv+->$`: a convolution sequence ending at a
terminal node. The supplied StableHLO example has two convolutions; the match
starting at operation 0 contains IDs 0 and 1 and a `+` count of 2. Suffix matches
can also be returned. Existing branching, common-node and strict-node semantics
still apply to aliases.

## Define a node

One JSON file defines one predicate:

```json
{
  "schema_version": 1,
  "name": "conv",
  "description": "Convolution operation",
  "formats": {
    "onnx": {"op_types": ["Conv"]},
    "stablehlo": {"op_types": ["stablehlo.convolution"]},
    "json": {"op_types": ["Conv", "stablehlo.convolution"]}
  }
}
```

`op_types` is an OR of exact original operation names, before punctuation is
normalized into raw predicate keys. ONNX custom-domain operations use
`domain.Op`. Names contain ASCII letters, digits or underscores. Description is
optional; the other fields are required. Unknown fields and rule kinds are
rejected. Version 1 supports JSON and operation types only. Version 2 adds an
optional `where` condition per format, evaluated by native C++ `NodeTest`.
Version 3 adds an optional [OpenMath symbol](#openmath-symbols). No version
supports YAML or composite subgraph definitions.

For example, a version 2 definition selects a declared 3-by-3 kernel:

```json
{
  "schema_version": 2,
  "name": "conv3x3",
  "formats": {
    "json": {
      "op_types": ["Conv"],
      "where": {
        "all": [
          {"path": ["attributes", "kernel_shape"], "eq": [3, 3]},
          {"path": ["name"], "regex": "encoder/conv_[0-9]+"},
          {"keys": ["attributes"], "regex": "debug_.*", "count": {"eq": 0}}
        ]
      }
    }
  }
}
```

The imported node is the rule document: `op`, `name`, `attributes`, `inputs`
and other metadata can be addressed by paths. See [NodeTest rules](node-rules.md)
for operators, presence semantics and the rule API. The native engine reads and
validates definition files, resolves `op_types`, and compiles and evaluates the
selected format's rules before graph matching. Python only imports the source
graph and transports its node documents. Invalid rules report the definition file, predicate and rule
path, including when no source operation is a candidate. Format rules for other
input formats are not compiled during this invocation.

## OpenMath symbols

Format mappings say how each IR spells an operation. A schema 3 definition can
also state what the operation *means*, independently of any IR, with an
[OpenMath](https://openmath.org/) symbol: a content dictionary (CD) and a symbol
name, such as `arith1.plus` or `transc1.ln`.

```json
{
  "schema_version": 3,
  "name": "add",
  "openmath": {"cd": "arith1", "name": "plus"},
  "formats": {
    "onnx": {"op_types": ["Add"]},
    "stablehlo": {"op_types": ["stablehlo.add"]}
  }
}
```

| Field | Meaning |
| --- | --- |
| `cd` | Content dictionary, `[A-Za-z][A-Za-z0-9_]*` (`arith1`, `transc1`, `relation1`). |
| `name` | Symbol in that CD, `[A-Za-z][A-Za-z0-9_]*` (`plus`, `unary_minus`). |
| `cdbase` | Optional nonempty URI of a non-standard CD collection; omit it for the official CDs. |

Other fields are rejected, and `openmath` is rejected before schema 3. The
names accept the `[A-Za-z][A-Za-z0-9_]*` subset of OpenMath names.

The symbol documents meaning only. It does not change matching: membership still
comes from `op_types` and `where`, and a source operation literally named
`arith1.plus` is not selected by it. It does not prove numerical equivalence
either: OpenMath symbols are scalar or generic, while IR operations add dtypes,
broadcasting and rounding. Give a symbol only when the meaning is exact; omit it
otherwise. For example, `arith1.root` takes the degree as a second argument, so
unary `sqrt` has none, and convolution has no standard symbol. The symbol is
listed by `--list-definitions` and reported in `node_definitions` (null when
absent).

A complete example and verification script are available in
[conditional matching examples](../examples/matches/README.md#conditional-nodes-with-nodetest).

Pass `--node-library` repeatedly for several files or directories. Directories
load their immediate `*.json` files in sorted order, without recursion; other
files and subdirectories are ignored, and a directory without definitions is an
error. A file argument must have a `.json` extension. Duplicate
predicate names are errors, including the same file given twice. An explicit library predicate overrides a raw
predicate with the same name for that invocation. Multiple predicates may match
the same source node; definitions do not mutate the global NodeDictionary.

A referenced predicate lacking a mapping for the selected format is an error.
A valid mapping whose operations are absent simply matches no nodes. Unused
definitions may omit the current format. Additional format names can be stored,
but importing those formats still requires an importer: the CLI currently reads
JSON graphs, ONNX and StableHLO, not OpenVX.

Definition files are strict UTF-8 JSON (a leading BOM is accepted). Every
error starts with the absolute definition file and, for rules, names the
predicate. The native reader also rejects duplicate object keys, `NaN`/`Infinity`
and integers outside 64 bits; the former Python reader silently kept the last
duplicate key or failed later while encoding such numbers.

## Library directories

A node library is any directory supplied explicitly on the command line; there
is no search path, environment variable or global registry. The repository keeps
its data separate from the engine sources:

| Directory | Role |
| --- | --- |
| `examples/library/nodes/` | Reusable base vocabulary (11 operation families) |
| `examples/library/conditional-nodes/` | Demonstration of version 2 `where` rules |
| `examples/matches/`, `examples/rewrite/` | Self-contained demonstration fixtures |

Project-specific libraries can live anywhere, for example `libraries/nodes/hardware/`
in a consumer repository, and are loaded with `--node-library` like the bundled one.

## Native C++ API

`NodeTest/NodeLibrary.hpp` exposes the same behavior without Python:

```cpp
#include "NodeTest/NodeLibrary.hpp"

auto library = CorGIReg::NodeLibrary::load({"examples/library/nodes"});
// documents: source node ID -> NodeValue object with at least {"op": ...}
auto memberships = library.resolve("stablehlo", documents);
CorGIReg::NodePredicates predicates;  // raw predicates first, if any
CorGIReg::addDefinitionPredicates(predicates, memberships);
auto result = CorGIReg::PGInterpreterTG("conv+->$").interpret()->test(graph, starts, predicates);
```

`resolve` returns, per definition, the matching source IDs or `std::nullopt`
when the definition has no mapping for that format. `NodeDefinition::fromDocument`
validates an already parsed document; `parseJson`/`writeJson` in
`NodeTest/NodeValueJson.hpp` read and write node documents. The library object
owns its definitions and is passed explicitly; it never registers predicates in
the process-wide `NodeDictionary`.

The native worker exposes the same operations for tooling:
`corgireg-match --node-library DIR --list-definitions` validates a library and
prints it as JSON. See the [worker protocol](native-protocol.md).

## Generating graphs from library keys

`corgireg-generate --node-library DIR [--format NAME]` (default format `json`)
replaces each pattern key that names a definition with that definition's first
`op_type` for the format; node names keep the key (`conv_0`). The result matches
the same pattern when loaded with the same library:

```sh
./build/corgireg-generate -r 'conv+->negate' --random --seed 7 --node-library examples/library/nodes -o graph.json
corgireg -i graph.json -r 'conv+->negate' --node-library examples/library/nodes
```

Keys without a definition stay raw operation names. A definition without a
mapping for the selected format, or with a `where` rule for it, is an error:
generation only produces an operation name, so it cannot guarantee a condition.

## Results and pattern reuse

Patterns can be kept as ordinary UTF-8 text files and selected with `-R`.
There is no additional named-pattern registry. The example library includes
11 operation definitions plus a convolution pattern file. See the
[library catalog](../examples/library/README.md) and
[runnable matching scripts](../examples/matches/README.md).

The result retains the complete query, source graph metadata, captured IDs,
names and quantifier traces. Captured predicate names are the names used in the
pattern. The additional `node_definitions` object records each definition's
description, absolute source file, OpenMath symbol, selected format, rule and matching node IDs
(`null` for a missing format mapping). This identifies the vocabulary used for
the result. Output cannot overwrite a loaded definition file.
