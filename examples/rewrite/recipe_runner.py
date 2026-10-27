"""External recipe prototype: use CorGIReg commands, own graph replacement here."""
import argparse
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from corgireg_cli.graph import validate_graph
from corgireg_cli.cli import write_report


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fields(value, required, optional=()):
    require(isinstance(value, dict), "Expected an object")
    require(set(required) <= value.keys() and not value.keys() - set(required) - set(optional),
            f"Expected fields {required}; optional {optional}")


def integer(value, minimum=0, maximum=1000000):
    require(type(value) is int and minimum <= value <= maximum, "Invalid integer")
    return value


def validate_recipe(recipe):
    fields(recipe, ["schema_version", "name", "match", "replacement", "inputs", "outputs"], ["conditions"])
    require(type(recipe["schema_version"]) is int and recipe["schema_version"] == 1, "Unsupported recipe version")
    for key in ("name", "match"):
        require(isinstance(recipe[key], str) and recipe[key].strip(), f"Empty {key}")
    replacement = recipe["replacement"]
    fields(replacement, ["pattern", "repetitions", "default_output_port"])
    require(isinstance(replacement["pattern"], str) and replacement["pattern"].strip(), "Empty replacement pattern")
    require(isinstance(replacement["repetitions"], dict), "repetitions must be an object")
    for key in replacement["repetitions"]:
        require(isinstance(key, str) and key.isascii() and key.isdecimal() and str(int(key)) == key,
                "Quantifier keys must be canonical positive integers")
        integer(int(key), 1)
    integer(replacement["default_output_port"], 1, 2**32 - 1)
    for direction in ("inputs", "outputs"):
        require(isinstance(recipe[direction], list), f"{direction} must be an array")
        for binding in recipe[direction]:
            fields(binding, ["match", "replacement"])
            for domain, label in (("match", "predicate"), ("replacement", "op")):
                selector = binding[domain]
                fields(selector, [label, "port"], ["position"])
                require(isinstance(selector[label], str) and selector[label], "Empty endpoint selector")
                integer(selector["port"], 1, 2**32 - 1)
                require(selector.get("position", "unique") in ("unique", "first", "last"), "Invalid position")
    require(isinstance(recipe.get("conditions", []), list), "conditions must be an array")
    for condition in recipe.get("conditions", []):
        fields(condition, ["op", "value"])
        require(condition["op"] == "even", "Unsupported condition")


def evaluate(expression, match, depth=0):
    """Bounded integer expressions; never eval Python or execute recipe text."""
    require(depth <= 32, "Expression nesting exceeds 32")
    if type(expression) is int:
        return integer(expression)
    require(isinstance(expression, dict), "Expected integer expression")
    if "quantifier" in expression:
        fields(expression, ["quantifier"], ["invocation"])
        index = integer(expression["quantifier"], 1)
        values = [q["count"] for walk in match["walks"] for q in walk["quantifiers"] if q["index"] == index]
        if "invocation" in expression:
            require(len(match["walks"]) == 1, "Invocation order requires one walk")
            invocation = integer(expression["invocation"])
            require(invocation < len(values), "Missing quantifier invocation")
            return integer(values[invocation])
        require(len(values) == 1, "Quantifier reference must identify exactly one invocation")
        return integer(values[0])
    fields(expression, ["op", "args"])
    require(isinstance(expression["args"], list) and len(expression["args"]) == 2, "Expected two expression arguments")
    left, right = [evaluate(arg, match, depth + 1) for arg in expression["args"]]
    operations = {"add": lambda: left + right, "subtract": lambda: left - right, "multiply": lambda: left * right}
    require(expression["op"] in operations, "Unsupported integer operation")
    return integer(operations[expression["op"]]())


def select(selector, nodes, label):
    choices = list(dict.fromkeys(node["id"] for node in nodes if node[label] == selector[label]))
    require(choices, f"Endpoint not found: {selector}")
    position = selector.get("position", "unique")
    if position == "unique":
        require(len(choices) == 1, f"Ambiguous endpoint: {selector}")
    return choices[-1] if position == "last" else choices[0]


def records(graph):
    bindings = {}
    for connection in graph.get("connections", []):
        bindings.setdefault((connection["from"], connection["to"]), []).append(connection)
    return [dict(c) for source, target in graph["edges"] for c in bindings.get((source, target),
            [{"from": source, "to": target, "input": None, "output": None}])]


def run_json(command):
    process = subprocess.run(list(map(str, command)), capture_output=True, text=True, encoding="utf-8", timeout=60)
    require(process.returncode == 0, process.stderr.strip() or "CorGIReg command failed")
    return json.loads(process.stdout)


def resolve(recipe, match):
    values = {key: ([evaluate(x, match) for x in value] if isinstance(value, list) else [evaluate(value, match)])
              for key, value in recipe["replacement"]["repetitions"].items()}
    require(all(values.values()), "Empty repetition list")
    captures = [node for walk in match["walks"] for node in walk["nodes"]]
    endpoints = {}
    for direction in ("inputs", "outputs"):
        endpoints[direction] = []
        for binding in recipe[direction]:
            selector = binding["match"]
            if selector.get("position", "unique") != "unique":
                require(len(match["walks"]) == 1, "Endpoint order requires one walk")
            endpoints[direction].append((select(selector, captures, "predicate"), selector["port"]))
    return values, endpoints


def splice(graph, fragment, recipe, matched, endpoints):
    removed = set(matched)
    require(removed, "Cannot replace an empty match")
    first = max((node["id"] for node in graph["nodes"]), default=-1) + 1
    require(first + len(fragment["nodes"]) <= 2**64, "Generated node IDs overflow")
    remap = {node["id"]: first + i for i, node in enumerate(fragment["nodes"])}
    nodes = [copy.deepcopy(node) for node in graph["nodes"] if node["id"] not in removed]
    for node in fragment["nodes"]:
        nodes.append({**node, "id": remap[node["id"]], "name": recipe["name"] + "/" + node["name"]})
    maps = {}
    for direction in ("inputs", "outputs"):
        maps[direction] = {}
        for key, binding in zip(endpoints[direction], recipe[direction]):
            require(key not in maps[direction], "Duplicate boundary mapping")
            target = binding["replacement"]
            maps[direction][key] = (remap[select(target, fragment["nodes"], "op")], target["port"])
    connections = []
    for c in records(fragment):
        connections.append({**c, "from": remap[c["from"]], "to": remap[c["to"]],
                            "output": c["output"] or recipe["replacement"]["default_output_port"]})
    seen = {"inputs": set(), "outputs": set()}
    for c in records(graph):
        source_inside, target_inside = c["from"] in removed, c["to"] in removed
        if source_inside and target_inside:
            continue
        if target_inside:
            key = (c["to"], c["input"])
            require(key in maps["inputs"], f"Unmapped incoming connection: {c}")
            require(key not in seen["inputs"], "Matched input has multiple producers")
            seen["inputs"].add(key)
            target, port = maps["inputs"][key]
            connections.append({**c, "to": target, "input": port})
        elif source_inside:
            key = (c["from"], c["output"])
            require(key in maps["outputs"], f"Unmapped outgoing connection: {c}")
            seen["outputs"].add(key)
            source, port = maps["outputs"][key]
            connections.append({**c, "from": source, "output": port})
        else:
            connections.append(c)
    for direction in seen:
        require(seen[direction] == set(maps[direction]), "Recipe boundary mapping was not used")
    occupied = {}
    for c in connections:
        if c["input"] is not None:
            key, producer = (c["to"], c["input"]), (c["from"], c["output"])
            require(key not in occupied or occupied[key] == producer, "Replacement input collision")
            occupied[key] = producer
    return validate_graph({"nodes": nodes, "edges": sorted({(c["from"], c["to"]) for c in connections}),
                           "connections": connections}), remap


def apply_recipe(graph, recipe, matcher, generator):
    validate_recipe(recipe)
    graph = validate_graph(copy.deepcopy(graph))
    with tempfile.TemporaryDirectory(prefix="corgireg-recipe-") as folder:
        source = Path(folder) / "source.json"
        source.write_text(json.dumps(graph), encoding="utf-8")
        report = run_json([sys.executable, "-m", "corgireg_cli", "-i", source, "-r", recipe["match"], "--engine", matcher])
        candidates = {}
        for match in report["matches"]:
            if not all(evaluate(c["value"], match) % 2 == 0 for c in recipe.get("conditions", [])):
                continue
            key = tuple(sorted(match["node_ids"]))
            resolved = resolve(recipe, match)
            require(key not in candidates or candidates[key] == resolved, "Ambiguous capture values for the same subgraph")
            candidates[key] = resolved
        if not candidates:
            return graph, {"recipe": recipe["name"], "changed": False}
        require(len(candidates) == 1, "Recipe requires exactly one matching subgraph")
        matched, (values, endpoints) = next(iter(candidates.items()))
        configuration = Path(folder) / "replacement.values"
        configuration.write_text("mode = deterministic\n" + "".join(
            f"{key} = {', '.join(map(str, counts))}\n" for key, counts in values.items()), encoding="utf-8")
        fragment = validate_graph(run_json([generator, "-r", recipe["replacement"]["pattern"], "-v", configuration]))
        result, remap = splice(graph, fragment, recipe, matched, endpoints)
        return result, {"recipe": recipe["name"], "changed": True, "matched_node_ids": list(matched),
                        "replacement_node_ids": list(remap.values()), "repetitions": values}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipe", type=Path, required=True)
    parser.add_argument("-i", "--input", type=Path, required=True)
    parser.add_argument("-o", "--output", required=True)
    parser.add_argument("--matcher", required=True)
    parser.add_argument("--generator", required=True)
    args = parser.parse_args()
    try:
        graph = json.loads(args.input.read_text(encoding="utf-8-sig"))
        recipe = json.loads(args.recipe.read_text(encoding="utf-8-sig"))
        result, report = apply_recipe(graph, recipe, args.matcher, args.generator)
        write_report(args.output, result)
        print(json.dumps(report), file=sys.stderr)
        return 0
    except (ValueError, OSError, subprocess.TimeoutExpired) as error:
        print(f"recipe: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
