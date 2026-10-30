"""List a source graph's operations and the library definitions covering each one.

Usage, from the repository root:
    PYTHONPATH=python python3 .agents/skills/corgireg-node-library/inventory.py MODEL
        [--format json|onnx|stablehlo] [--function NAME] [--node-library DIR]...
        [--engine build/corgireg-match]

Prints JSON: format, then one entry per original operation name with its node
count, the definitions whose op_types list it, and whether all of them have a
"where" rule (coverage then depends on node attributes).
"""
import argparse
import collections
import json
import sys
from pathlib import Path

from corgireg_cli.graph import load_json, validate_graph
from corgireg_cli.node_library import load_definitions

FORMATS = {".json": "json", ".onnx": "onnx", ".mlir": "stablehlo", ".stablehlo": "stablehlo"}


def load_graph(path, format_name, function):
    if format_name == "json":
        return load_json(path)
    if format_name == "onnx":
        from corgireg_cli.import_onnx import load_onnx
        return load_onnx(path)
    from corgireg_cli.import_stablehlo import load_stablehlo
    return load_stablehlo(path, function)


def main(argv=None):
    cli = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    cli.add_argument("model", type=Path)
    cli.add_argument("--format", choices=["json", "onnx", "stablehlo"])
    cli.add_argument("--function")
    cli.add_argument("--node-library", action="append", type=Path, default=[])
    cli.add_argument("--engine", type=Path)
    args = cli.parse_args(argv)
    format_name = args.format or FORMATS.get(args.model.suffix.lower())
    if not format_name:
        cli.error("cannot infer the format; pass --format")
    graph = validate_graph(load_graph(args.model, format_name, args.function))
    counts = collections.Counter(node["op"] for node in graph["nodes"])
    definitions = load_definitions(args.node_library, args.engine) if args.node_library else {}
    operations = {}
    for op, count in sorted(counts.items()):
        covering = [name for name, definition in sorted(definitions.items())
                    if op in definition["formats"].get(format_name, {}).get("op_types", [])]
        # Conditional only when every covering definition has a "where" rule.
        conditional = bool(covering) and all("where" in definitions[name]["formats"][format_name] for name in covering)
        operations[op] = {"count": count, "definitions": covering, "conditional": conditional}
    json.dump({"format": format_name, "operations": operations}, sys.stdout, indent=2)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
