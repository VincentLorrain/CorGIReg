# Node library

Load `nodes/` with `--node-library examples/library/nodes`, for matching
(`corgireg`) or generation (`corgireg-generate`). Each JSON file is an
independent, editable definition; you can also load individual files. The C++
engine loads them natively (`NodeLibrary`). See the
[schema and behavior](../../doc/node-library.md) and [runnable matching scripts](../matches/README.md).

| Predicate | ONNX operation | StableHLO operation |
| --- | --- | --- |
| conv | Conv | stablehlo.convolution |
| abs | Abs | stablehlo.abs |
| negate | Neg | stablehlo.negate |
| add | Add | stablehlo.add |
| subtract | Sub | stablehlo.subtract |
| multiply | Mul | stablehlo.multiply |
| divide | Div | stablehlo.divide |
| exponential | Exp | stablehlo.exponential |
| log | Log | stablehlo.log |
| sqrt | Sqrt | stablehlo.sqrt |
| tanh | Tanh | stablehlo.tanh |

JSON input accepts either column's operation spelling. These base rules classify
operation families; they do not check attributes, tensor types, broadcasting,
numerical equivalence or composite implementations. For example, an activation
implemented as several operations requires a graph pattern, not a type alias.

The separate `conditional-nodes/` directory contains version 2 definitions with
attribute and argument constraints. Load it with an additional `--node-library`
option. [`conv3x3.json`](conditional-nodes/conv3x3.json) demonstrates value checks,
presence checks and regexes on values and keys for JSON and ONNX inputs.

`patterns/conv-chain.txt` is a reusable pattern; `conv.mlir` is a real StableHLO
fixture with two convolutions. Patterns and node definitions are stored separately
so one vocabulary can be reused across multiple patterns and input formats.
