import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from recipe_runner import apply_recipe, records, validate_recipe, evaluate as evaluate_expression

HERE = Path(__file__).resolve().parent


def load(name):
    return json.loads((HERE / name).read_text(encoding="utf-8"))


def evaluate(graph, node, x, y):
    operator = next(n["op"] for n in graph["nodes"] if n["id"] == node)
    if operator == "X":
        return x
    if operator == "Y":
        return y
    args = {c["input"]: evaluate(graph, c["from"], x, y) for c in records(graph) if c["to"] == node}
    if operator == "Sub":
        return args[1] - args[2]
    if operator == "Neg":
        return -args[1]
    if operator in ("Add", "Sum"):
        return args[1] + args[2]
    return next(iter(args.values()))


class RecipeTests(unittest.TestCase):
    def setUp(self):
        self.graph = load("network.json")
        self.recipe = load("recipes/subtract.json")
        self.matcher = os.environ["CORGIREG_ENGINE"]
        self.generator = os.environ["CORGIREG_GENERATOR"]

    def apply(self, graph=None, recipe=None):
        return apply_recipe(graph if graph is not None else self.graph,
                            recipe if recipe is not None else self.recipe, self.matcher, self.generator)

    def test_json_recipe_matches_generates_and_reconnects_with_exact_ports(self):
        original = copy.deepcopy(self.graph)
        transformed, report = self.apply()
        self.assertEqual(self.graph, original)
        self.assertEqual(report["matched_node_ids"], [30])
        self.assertEqual(report["replacement_node_ids"], [61, 62])
        self.assertEqual({n["id"]: n["op"] for n in transformed["nodes"]},
                         {10: "X", 20: "Y", 40: "Out", 50: "Side", 60: "Out", 61: "Neg", 62: "Add"})
        self.assertEqual({(c["from"], c["to"], c["input"], c["output"]) for c in records(transformed)},
                         {(10, 62, 1, 1), (20, 61, 1, 2), (61, 62, 2, 1),
                          (62, 40, 1, 1), (62, 60, 3, 1), (10, 50, 1, 1)})
        self.assertEqual(next(n for n in transformed["nodes"] if n["id"] == 50)["attributes"], {"keep": True})
        for x in (-8, 0, 3, 10):
            for y in (-5, 0, 2, 7):
                for output in (40, 50, 60):
                    self.assertEqual(evaluate(original, output, x, y), evaluate(transformed, output, x, y))

    def test_multi_node_match_drives_replacement_repetition_expression(self):
        graph = {"nodes": [{"id": i, "op": op} for i, op in [(10,"X"),(30,"Neg"),(31,"Neg"),(40,"Out")]],
                 "edges": [[10,30],[30,31],[31,40]],
                 "connections": [{"from": a, "to": b, "input": 1, "output": 1}
                                 for a, b in [(10,30),(30,31),(31,40)]]}
        result, report = self.apply(graph, load("recipes/negate-chain.json"))
        self.assertEqual(report["matched_node_ids"], [30, 31])
        self.assertEqual(report["repetitions"], {"1": [1]})
        self.assertEqual(report["replacement_node_ids"], [41])
        self.assertEqual(result["edges"], [(10, 41), (41, 40)])
        self.assertEqual(next(n for n in result["nodes"] if n["id"] == 41)["op"], "Identity")
        for x in (-7, 0, 3, 19):
            self.assertEqual(evaluate(result, 40, x, 0), evaluate(graph, 40, x, 0))

    def test_different_operator_recipe_needs_no_runner_code_change(self):
        self.graph["nodes"][2]["op"] = "Add"
        self.recipe["match"] = "Add#"
        self.recipe["replacement"]["pattern"] = "Sum#"
        for direction in ("inputs", "outputs"):
            for binding in self.recipe[direction]:
                binding["match"]["predicate"] = "Add"
                binding["replacement"]["op"] = "Sum"
                binding["replacement"]["port"] = binding["match"]["port"]
        result, report = self.apply()
        self.assertTrue(report["changed"])
        self.assertEqual(evaluate(result, 40, 7, 2), 9)
        self.assertEqual([n["op"] for n in result["nodes"] if n["id"] > 60], ["Sum"])

    def test_missing_match_is_noop(self):
        self.recipe["match"] = "Absent#"
        result, report = self.apply()
        self.assertFalse(report["changed"])
        self.assertEqual({n["id"] for n in result["nodes"]}, {10,20,30,40,50,60})
        self.assertEqual(set(result["edges"]), {tuple(e) for e in self.graph["edges"]})

    def test_unmapped_boundaries_and_collisions_are_rejected_without_mutation(self):
        for fault in ("missing_input", "missing_output", "extra_output", "collision", "duplicate_mapping"):
            graph, recipe = copy.deepcopy(self.graph), copy.deepcopy(self.recipe)
            if fault == "missing_input": recipe["inputs"].pop()
            if fault == "missing_output": recipe["outputs"].clear()
            if fault == "extra_output": graph["connections"][2]["output"] = 2
            if fault == "collision": recipe["inputs"][0]["replacement"]["port"] = 2
            if fault == "duplicate_mapping": recipe["inputs"].append(copy.deepcopy(recipe["inputs"][0]))
            original = copy.deepcopy(graph)
            with self.subTest(fault=fault), self.assertRaises(ValueError):
                self.apply(graph, recipe)
            self.assertEqual(graph, original)

    def test_multiple_matches_id_overflow_and_bad_expressions_are_rejected(self):
        self.graph["nodes"].append({"id": 70, "op": "Sub"})
        with self.assertRaisesRegex(ValueError, "exactly one matching"):
            self.apply()
        self.graph["nodes"][-1] = {"id": 2**64 - 1, "op": "Unrelated"}
        with self.assertRaisesRegex(ValueError, "overflow"):
            self.apply()
        self.graph = load("network.json")
        self.recipe["replacement"]["repetitions"] = {"1": {"quantifier": 99}}
        with self.assertRaisesRegex(ValueError, "exactly one invocation"):
            self.apply()

    def test_schema_and_cli_failure_preserve_existing_output(self):
        for value in (2, "1", True):
            recipe = copy.deepcopy(self.recipe)
            recipe["schema_version"] = value
            with self.assertRaises(ValueError): validate_recipe(recipe)
        with tempfile.TemporaryDirectory(dir=HERE.parents[1] / "build") as folder:
            output, recipe = Path(folder) / "output.json", Path(folder) / "recipe.json"
            output.write_text("keep", encoding="utf-8")
            self.recipe["inputs"].clear()
            recipe.write_text(json.dumps(self.recipe), encoding="utf-8")
            process = subprocess.run([sys.executable, HERE / "recipe_runner.py", "--recipe", recipe,
                                      "-i", HERE / "network.json", "-o", output,
                                      "--matcher", self.matcher, "--generator", self.generator],
                                     capture_output=True, text=True, timeout=30)
            self.assertEqual(process.returncode, 2, process.stderr)
            self.assertEqual(output.read_text(encoding="utf-8"), "keep")

    def test_cli_writes_a_graph_that_can_be_matched_again(self):
        with tempfile.TemporaryDirectory(dir=HERE.parents[1] / "build") as folder:
            output = Path(folder) / "transformed.json"
            command = [sys.executable, HERE / "recipe_runner.py", "--recipe", HERE / "recipes/subtract.json",
                       "-i", HERE / "network.json", "-o", output,
                       "--matcher", self.matcher, "--generator", self.generator]
            process = subprocess.run(command, capture_output=True, text=True, timeout=30)
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(json.loads(process.stderr)["matched_node_ids"], [30])
            self.assertTrue(output.is_file())
            for query, expected in [("Sub#", set()), ("Neg-2->Add#", {(61, 62)})]:
                matched = subprocess.run([sys.executable, "-m", "corgireg_cli", "-i", output,
                                          "-r", query, "--engine", self.matcher],
                                         capture_output=True, text=True, timeout=30)
                self.assertEqual(matched.returncode, 0, matched.stderr)
                self.assertEqual({tuple(m["node_ids"]) for m in json.loads(matched.stdout)["matches"]}, expected)

    def test_expression_errors_are_explicit_and_odd_chain_is_unchanged(self):
        match = {"walks": [{"quantifiers": [{"index": 1, "count": 2}]}]}
        for expression in ({"op": [], "args": [1, 2]}, {"op": "subtract", "args": [1, 2]},
                           {"op": "multiply", "args": [1000000, 1000000]}, {"quantifier": 1, "invocation": 4}):
            with self.subTest(expression=expression), self.assertRaises(ValueError):
                evaluate_expression(expression, match)
        graph = {"nodes": [{"id": 1, "op": "X"}, {"id": 2, "op": "Neg"}, {"id": 3, "op": "Out"}],
                 "edges": [[1, 2], [2, 3]], "connections": [
                     {"from": 1, "to": 2, "input": 1, "output": 1},
                     {"from": 2, "to": 3, "input": 1, "output": 1}]}
        result, report = self.apply(graph, load("recipes/negate-chain.json"))
        self.assertFalse(report["changed"])
        self.assertEqual([n["op"] for n in result["nodes"]], ["X", "Neg", "Out"])


if __name__ == "__main__":
    unittest.main()
