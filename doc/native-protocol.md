# Internal native-worker protocol

`corgireg-match` reads a token stream from stdin and writes native JSON to stdout.
The public `corgireg` Python CLI imports files, encodes this stream and enriches
the reply. Applications should prefer the public CLI or C++ API; this internal
transport is not the [public JSON report](match-report-json.md).

The worker accepts an optional process argument `--jobs N` (1..256, default 1).
It controls [parallel traversal](multithreading.md) without changing the stdin
protocol or public report schema.

`--node-library PATH` (repeatable) and `--format NAME` (default `json`) make the
worker load and resolve [node definitions](node-library.md) natively. The request
must then be version 3 (or 6) with a definition count of 0: the worker resolves
definitions from the node documents, and its reply contains the public
`node_definitions` object instead of `definition_nodes`. This is what the public
CLI sends. `--list-definitions` validates the libraries, prints them as JSON
(name to `schema_version`, `name`, `description`, `formats`, `source`) and exits
without reading stdin.

Whitespace separates tokens. Text tokens are hexadecimal UTF-8 bytes; `-`
represents an empty string. IDs are unsigned 64-bit integers. The query is
encoded text; Python normalizes its whitespace for the native lexer while
retaining the original query in the public report.

## Request versions

All requests start with `CORGiReg1`, `CORGiReg2` or `CORGiReg3`, followed by:

1. Node count, then one record per node: `id hex_predicate hex_name`.
   Version 3 additionally carries a typed node document after each record.
2. Edge count, then `source_id target_id` for each directed edge.
3. For versions 2 and 3 only: definition count and definition records below.
4. Hex-encoded complete query, then maximum number of starting tuples.

A definition starts with `hex_name available`, where available is 0 or 1.
If available is 0, there are no more fields for that definition. Otherwise it
has a membership count followed by source IDs. Version 3 then appends a typed
`where` rule (`n` if absent) and the hex-encoded definition source filename.
Version 1 uses raw predicates; version 2 transports alias memberships; version
3 also supports native rule evaluation and diagnostics identifying the file.
Versions 2 and 3 with caller-resolved memberships remain accepted for existing
clients; the public CLI no longer resolves definitions itself.

## Version 3 typed values

| Tokens | Value |
| --- | --- |
| `n` | Null |
| `m` | Internal missing value, including omitted input slots |
| `b 0` / `b 1` | Boolean |
| `i INTEGER` | Signed 64-bit integer |
| `u INTEGER` | Unsigned 64-bit integer |
| `d NUMBER` | Finite double |
| `s HEX` | UTF-8 string |
| `a COUNT VALUE...` | Array of typed values |
| `o COUNT HEX_KEY VALUE...` | Object of key/value pairs |

Collections are limited to one million entries and nesting to 64 levels.
Malformed values, duplicate object keys and trailing request tokens are rejected.
Missing is an internal value, not a JSON literal. The Python adapter preserves
the original source document in the public report.

## Reply and errors

A successful native reply has `query` and `matches`, using the match fields
described in [the report reference](match-report-json.md). Rule requests also
return `definition_nodes`, mapping aliases to filtered source IDs. With
`--node-library`, the worker returns the complete public `node_definitions`
object instead, which Python copies into the report.
The worker does not add source path, format, graph or report schema version.

Worker failure is indicated by a nonzero process exit status and stderr text.
The public CLI maps these failures and subprocess timeouts to exit code 2.
Do not parse diagnostic wording as a stable machine-readable error schema.

Implementation references: [encoder](../python/corgireg_cli/graph.py),
[native request reader](../src/Cli/MatchRequest.cpp),
[worker entry point](../src/Cli/main.cpp).
# Numbered connections extension

`CORGiReg4`, `CORGiReg5`, and `CORGiReg6` extend versions 1, 2, and 3 respectively.
All fields keep the corresponding base-version layout, except connection records:
each contains `from to input_port output_port`. Zero encodes an unspecified port;
positive ports must fit 32 bits. The edge count counts connection records, so
several records may share the same pair of node IDs. Their adjacency is deduplicated
by the engine. Legacy versions 1–3 remain accepted unchanged.

