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
rejected. Version 1 supports JSON and operation types only: no YAML, attribute
conditions or composite subgraph definitions.

Pass `--node-library` repeatedly for several files or directories. Directories
load their immediate `*.json` files in sorted order, without recursion. Duplicate
predicate names are errors. An explicit library predicate overrides a raw
predicate with the same name for that invocation. Multiple predicates may match
the same source node; definitions do not mutate the global NodeDictionary.

A referenced predicate lacking a mapping for the selected format is an error.
A valid mapping whose operations are absent simply matches no nodes. Unused
definitions may omit the current format. Additional format names can be stored,
but importing those formats still requires an importer: the CLI currently reads
JSON graphs, ONNX and StableHLO, not OpenVX.

## Results and pattern reuse

Patterns can be kept as ordinary UTF-8 text files and selected with `-R`.
There is no additional named-pattern registry. The example library includes
`conv`, `negate`, and `abs` definitions plus a convolution pattern file.

The result retains the complete query, source graph metadata, captured IDs,
names and quantifier traces. Captured predicate names are the names used in the
pattern. The additional `node_definitions` object records each definition's
description, absolute source file, selected format, rule and matching node IDs
(`null` for a missing format mapping). This identifies the vocabulary used for
the result. Output cannot overwrite a loaded definition file.
