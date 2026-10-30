# Node library

Load `nodes/` with `--node-library examples/library/nodes`, for matching
(`corgireg`) or generation (`corgireg-generate`). Each JSON file is an
independent, editable definition; you can also load individual files. The C++
engine loads them natively (`NodeLibrary`). See the
[schema and behavior](../../doc/node-library.md) and [runnable matching scripts](../matches/README.md).

| Predicate | ONNX operation | StableHLO operation | OpenMath symbol |
| --- | --- | --- | --- |
| conv | Conv | stablehlo.convolution | — |
| abs | Abs | stablehlo.abs | arith1.abs |
| negate | Neg | stablehlo.negate | arith1.unary_minus |
| add | Add | stablehlo.add | arith1.plus |
| subtract | Sub | stablehlo.subtract | arith1.minus |
| multiply | Mul | stablehlo.multiply | arith1.times |
| divide | Div | stablehlo.divide | arith1.divide |
| exponential | Exp | stablehlo.exponential | transc1.exp |
| log | Log | stablehlo.log | transc1.ln |
| sqrt | Sqrt | stablehlo.sqrt | — |
| tanh | Tanh | stablehlo.tanh | transc1.tanh |

JSON input accepts either operation spelling. The OpenMath symbol (schema 3)
states the format-independent meaning; `sqrt` (`arith1.root` takes a degree) and
`conv` have no exact standard symbol and omit it. These base rules classify
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
