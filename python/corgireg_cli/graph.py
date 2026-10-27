"""Validated operation graph shared by all file importers."""
import json
import re
import math

_ABSENT_INPUT = object()


def node_id(value):
    if type(value) is not int or not 0 <= value < 2**64:
        raise ValueError("Node IDs must be unsigned 64-bit integers")
    return value


def validate_graph(graph):
    if not isinstance(graph, dict) or not isinstance(graph.get("nodes"), list) or not isinstance(graph.get("edges"), list):
        raise ValueError("Graph requires nodes and edges arrays")
    nodes, ids, aliases = [], set(), {}
    for node in graph["nodes"]:
        if not isinstance(node, dict):
            raise ValueError("Each node must be an object")
        ident = node_id(node.get("id"))
        if ident in ids:
            raise ValueError(f"Duplicate node ID: {ident}")
        ids.add(ident)
        op = node.get("op")
        name = node.get("name", str(ident))
        if not isinstance(op, str) or not op or not isinstance(name, str):
            raise ValueError("Nodes require a nonempty op string and a string name")
        predicate = re.sub(r"[^A-Za-z_0-9]", "_", op)
        if predicate in aliases and aliases[predicate] != op:
            raise ValueError(f"Operation alias collision: {aliases[predicate]!r} and {op!r}")
        aliases[predicate] = op
        nodes.append({**node, "id": ident, "op": op, "name": name, "predicate": predicate})
    edges = set()
    for edge in graph["edges"]:
        if not isinstance(edge, (list, tuple)) or len(edge) != 2:
            raise ValueError("Edges must contain exactly two node IDs")
        source, target = map(node_id, edge)
        if source not in ids or target not in ids:
            raise ValueError(f"Edge references an unknown node: {edge}")
        edges.add((source, target))
    result = {"nodes": sorted(nodes, key=lambda node: node["id"]), "edges": sorted(edges)}
    if "connections" in graph:
        if not isinstance(graph["connections"], list):
            raise ValueError("connections must be an array")
        bindings = set()
        for binding in graph["connections"]:
            if not isinstance(binding, dict) or set(binding) - {"from", "to", "input", "output"}:
                raise ValueError("A connection requires from/to and optional input/output ports")
            source, target = node_id(binding.get("from")), node_id(binding.get("to"))
            if (source, target) not in edges:
                raise ValueError("A connection must reference an existing edge")
            ports = []
            for name in ("input", "output"):
                port = binding.get(name)
                if port is not None and (type(port) is not int or not 1 <= port < 2**32):
                    raise ValueError("Port numbers must be positive 32-bit integers or null")
                ports.append(port or 0)
            bindings.add((source, target, *ports))
        result["connections"] = [{"from": a, "to": b, "input": i or None, "output": o or None}
                                 for a, b, i, o in sorted(bindings)]
    return result


def load_json(path):
    return validate_graph(json.loads(path.read_text(encoding="utf-8-sig")))


def encode_value(value, depth=0):
    """Typed transport only: rules are compiled and evaluated by C++ NodeTest."""
    if depth > 64:
        raise ValueError("Node document nesting exceeds 64")
    if value is _ABSENT_INPUT:
        return "m"
    if value is None:
        return "n"
    if type(value) is bool:
        return f"b {int(value)}"
    if type(value) is int:
        if not -(2**63) <= value < 2**64:
            raise ValueError("Node integer is outside the supported 64-bit range")
        return f"{'i' if value < 0 else 'u'} {value}"
    if type(value) is float:
        if not math.isfinite(value):
            raise ValueError("Node numeric values must be finite")
        return f"d {value!r}"
    if isinstance(value, str):
        return "s " + (value.encode("utf-8").hex() or "-")
    if isinstance(value, (list, dict)):
        if len(value) > 1000000:
            raise ValueError("Node value collection exceeds 1000000 items")
        if isinstance(value, list):
            return f"a {len(value)} " + " ".join(encode_value(item, depth + 1) for item in value)
        if any(not isinstance(key, str) for key in value):
            raise ValueError("Node object keys must be strings")
        return f"o {len(value)} " + " ".join((key.encode("utf-8").hex() or "-") + " " +
                                              encode_value(item, depth + 1) for key, item in value.items())
    raise ValueError(f"Unsupported node value type: {type(value).__name__}")


def encode_request(graph, query, max_starts, memberships=None, definition_report=None):
    def text(value):
        return value.encode("utf-8").hex() or "-"
    version = "CORGiReg3" if definition_report is not None else ("CORGiReg1" if memberships is None else "CORGiReg2")
    ported = bool(graph.get("connections"))
    if ported:
        version = "CORGiReg" + str(int(version[-1]) + 3)
    lines = [version, str(len(graph["nodes"]))]
    for node in graph["nodes"]:
        lines.append(f'{node["id"]} {text(node["predicate"])} {text(node["name"])}')
        if definition_report is not None:
            document = dict(node)
            if isinstance(document.get("inputs"), list):
                document["inputs"] = [_ABSENT_INPUT if item == "" else item for item in document["inputs"]]
            lines.append(encode_value(document))
    bindings = {}
    for binding in graph.get("connections", []):
        bindings.setdefault((binding["from"], binding["to"]), []).append(binding)
    records = []
    for a, b in graph["edges"]:
        if ported:
            for binding in bindings.get((a, b), [{}]):
                records.append(f"{a} {b} {binding.get('input') or 0} {binding.get('output') or 0}")
        else:
            records.append(f"{a} {b}")
    lines.append(str(len(records)))
    lines.extend(records)
    if memberships is not None:
        lines.append(str(len(memberships)))
        for name, ids in sorted(memberships.items()):
            if ids is None:
                lines.append(f"{text(name)} 0")
            else:
                lines.append(f"{text(name)} 1 {len(ids)} " + " ".join(map(str, ids)))
                if definition_report is not None:
                    definition = definition_report[name]
                    lines.append(encode_value(definition["rule"].get("where")))
                    lines.append(text(definition["source"]))
    # The existing lexer accepts spaces; text files can contain CRLF and tabs.
    lines.extend([text(re.sub(r"\s", " ", query)), str(max_starts)])
    return "\n".join(lines) + "\n"
