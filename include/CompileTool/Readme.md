# Pattern compiler

The lexer and parser turn a graph pattern into an abstract syntax tree (AST).
The parser validates the pattern and expands a converging path at a shared
anchor into separate paths. `RootSelectionPass` can propose a shared starting
point for eligible paths.

`PGInterpreterTG` builds the transition graph used for matching.
`PGInterpreterIR` walks the same AST to generate example operation graphs.
Use the [architecture guide](../../doc/architecture.md) to follow these stages
and the [worked example](../../doc/walkthrough.md) to see how they affect a result.
