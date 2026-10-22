"""Validated operation graph shared by all file importers."""
import json
import re


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
    return {"nodes": sorted(nodes, key=lambda node: node["id"]), "edges": sorted(edges)}


def load_json(path):
    return validate_graph(json.loads(path.read_text(encoding="utf-8-sig")))


def encode_request(graph, query, max_starts, memberships=None):
    def text(value):
        return value.encode("utf-8").hex() or "-"
    lines = ["CORGiReg1" if memberships is None else "CORGiReg2", str(len(graph["nodes"]))]
    for node in graph["nodes"]:
        lines.append(f'{node["id"]} {text(node["predicate"])} {text(node["name"])}')
    lines.append(str(len(graph["edges"])))
    lines.extend(f"{a} {b}" for a, b in graph["edges"])
    if memberships is not None:
        lines.append(str(len(memberships)))
        for name, ids in sorted(memberships.items()):
            if ids is None:
                lines.append(f"{text(name)} 0")
            else:
                lines.append(f"{text(name)} 1 {len(ids)} " + " ".join(map(str, ids)))
    # The existing lexer accepts spaces; text files can contain CRLF and tabs.
    lines.extend([text(re.sub(r"\s", " ", query)), str(max_starts)])
    return "\n".join(lines) + "\n"
