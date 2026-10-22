# Node library

Load `nodes/` with `--node-library examples/library/nodes`. Each JSON file is an
independent, editable definition; you can also load individual files. See the
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

JSON input accepts either column's operation spelling. These rules classify
operation families; they do not check attributes, tensor types, broadcasting,
numerical equivalence or composite implementations. For example, an activation
implemented as several operations requires a graph pattern, not a type alias.

`patterns/conv-chain.txt` is a reusable pattern; `conv.mlir` is a real StableHLO
fixture with two convolutions. Patterns and node definitions are stored separately
so one vocabulary can be reused across multiple patterns and input formats.
