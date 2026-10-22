"""Declarative, format-specific definitions of semantic node predicates."""
import json
from pathlib import Path
import re


def load_definitions(paths):
    definitions = {}
    for location in paths:
        location = Path(location)
        files = sorted(location.glob("*.json")) if location.is_dir() else [location]
        if not files:
            raise ValueError(f"No node definitions found in {location}")
        for path in files:
            if path.suffix.lower() != ".json":
                raise ValueError(f"Node definitions must be JSON files: {path}")
            definition = json.loads(path.read_text(encoding="utf-8-sig"))
            if not isinstance(definition, dict) or set(definition) - {"schema_version", "name", "description", "formats"}:
                raise ValueError(f"Invalid node definition fields in {path}")
            if type(definition.get("schema_version")) is not int or definition["schema_version"] != 1:
                raise ValueError(f"Unsupported node definition schema in {path}")
            name = definition.get("name")
            if not isinstance(name, str) or re.fullmatch(r"[A-Za-z_0-9]+", name) is None:
                raise ValueError(f"Invalid predicate name in {path}")
            if name in definitions:
                raise ValueError(f"Duplicate node definition: {name}")
            if not isinstance(definition.get("description", ""), str):
                raise ValueError(f"Invalid description for {name}")
            formats = definition.get("formats")
            if not isinstance(formats, dict) or not formats:
                raise ValueError(f"Node {name} requires format mappings")
            for format_name, rule in formats.items():
                if not isinstance(format_name, str) or not format_name:
                    raise ValueError(f"Invalid format name for {name}")
                if not isinstance(rule, dict) or set(rule) != {"op_types"}:
                    raise ValueError(f"Only op_types rules are supported for {name}/{format_name}")
                types = rule["op_types"]
                if not isinstance(types, list) or not types or any(not isinstance(op, str) or not op for op in types):
                    raise ValueError(f"op_types must be a nonempty array of strings for {name}/{format_name}")
            definitions[name] = {**definition, "source": str(path.resolve())}
    return definitions


def resolve_definitions(definitions, graph, format_name):
    memberships, report = {}, {}
    for name, definition in sorted(definitions.items()):
        rule = definition["formats"].get(format_name)
        # Missing mapping differs from a valid mapping with no matching source node.
        ids = None if rule is None else [node["id"] for node in graph["nodes"] if node["op"] in rule["op_types"]]
        memberships[name] = ids
        report[name] = {"description": definition.get("description", ""), "source": definition["source"],
                        "format": format_name, "rule": rule, "node_ids": ids}
    return memberships, report
