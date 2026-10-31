import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python"))
from corgireg_cli.graph import validate_graph


class CliTests(unittest.TestCase):
    def test_inline_node_conditions_default_to_existing_attribute_names(self):
        graph = {"nodes": [{"id": 42, "op": "Conv", "attributes": {"kernel_shape": [3, 3], "group": 1}},
                           {"id": 99, "op": "Relu"}], "edges": [[42, 99]]}
        self.graph.write_text(json.dumps(graph), encoding="utf-8")
        for query, expected in [("Conv(kernel_shape=[3,3])->Relu", True),
                                ("Conv(kernel_shape=[5,5])->Relu", False),
                                ("Conv(kernel_shape=[3,3],group=1)->Relu", True),
                                ("Conv(kernel_shape=[3,3],group=2)->Relu", False),
                                ("Conv(missing=null)->Relu", False)]:
            for jobs in [1, 4]:
                with self.subTest(query=query, jobs=jobs):
                    process = self.invoke("-i", self.graph, "-r", query, "--jobs", jobs)
                    self.assertEqual(process.returncode, 0, process.stderr)
                    report = json.loads(process.stdout)
                    self.assertEqual(bool(report["matches"]), expected)
                    if expected:
                        self.assertEqual(report["matches"][0]["node_ids"], [42, 99])
        for query in ["Conv(kernel_shape=3x3)->Relu", "Conv(padding=SAME)->Relu"]:
            self.assertNotEqual(self.invoke("-i", self.graph, "-r", query).returncode, 0)

    def test_inline_node_conditions_translate_foreign_keys_to_json(self):
        definition = {"schema_version": 4, "name": "conv", "key_matching": {
            "kernel_shape": {"onnx": "onnx_kernel", "stablehlo": "filter_size"}},
            "formats": {"json": {"op_types": ["Conv"], "where": {"path": ["attributes", "group"], "eq": 1}}}}
        library = self.folder / "conv.json"
        library.write_text(json.dumps(definition), encoding="utf-8")
        graph = {"nodes": [{"id": 42, "op": "Conv", "attributes": {"kernel_shape": [3, 3], "group": 1}},
                           {"id": 99, "op": "Relu"}], "edges": [[42, 99]]}
        for group in [1, 2]:
            graph["nodes"][0]["attributes"]["group"] = group
            self.graph.write_text(json.dumps(graph), encoding="utf-8")
            for alias in ["kernel_shape", "onnx_kernel", "filter_size"]:
                process = self.invoke("-i", self.graph, "--node-library", library,
                                      "-r", f"conv({alias}=[3,3])->Relu")
                self.assertEqual(process.returncode, 0, process.stderr)
                report = json.loads(process.stdout)
                self.assertEqual(bool(report["matches"]), group == 1)
                self.assertEqual(report["node_definitions"]["conv"]["key_matching"], definition["key_matching"])

    def test_inline_filters_apply_to_each_repetition(self):
        graph = {"nodes": [{"id": 0, "op": "Conv", "attributes": {"group": 1}},
                           {"id": 1, "op": "Conv", "attributes": {"group": 1}},
                           {"id": 2, "op": "Relu"}], "edges": [[0, 1], [1, 2]]}
        for group, expected in [(1, True), (2, False)]:
            graph["nodes"][1]["attributes"]["group"] = group
            self.graph.write_text(json.dumps(graph), encoding="utf-8")
            process = self.invoke("-i", self.graph, "-r", "Conv(group=1)+->Relu")
            self.assertEqual(process.returncode, 0, process.stderr)
            matches = json.loads(process.stdout)["matches"]
            self.assertEqual(bool(matches), expected)
            if expected:
                full = next(m for m in matches if m["starts"] == [0])
                self.assertEqual(full["node_ids"], [0, 1, 2])
                self.assertEqual(full["walks"][0]["quantifiers"][0]["count"], 2)

    def test_numbered_connections_survive_json_and_parallel_matching(self):
        graph = {"nodes": [{"id": 0, "op": "X"}, {"id": 9, "op": "Sub"}],
                 "edges": [[0, 9]], "connections": [
                     {"from": 0, "to": 9, "input": 1, "output": 2},
                     {"from": 0, "to": 9, "input": 2, "output": 2}]}
        self.graph.write_text(json.dumps(graph), encoding="utf-8")
        for query, expected in [("X-1->Sub", True), ("X-2->Sub", True), ("X-3->Sub", False),
                                ("Sub<-1-X", True), ("X-*->Sub", True),
                                ("X#-1->Sub#;X#-2->Sub#", True)]:
            reference = None
            for jobs in [1, 4]:
                process = self.invoke("-i", self.graph, "-r", query, "--jobs", jobs)
                self.assertEqual(process.returncode, 0, process.stderr)
                report = json.loads(process.stdout)
                self.assertEqual(bool(report["matches"]), expected)
                self.assertEqual(report["graph"]["connections"], graph["connections"])
                if reference is not None:
                    self.assertEqual(report, reference)
                reference = report

    def test_connection_validation_rejects_bad_ports_and_dangling_bindings(self):
        for port in [0, -1, True, 1.5, "1", 2**32]:
            graph = {"nodes": [{"id": 0, "op": "A"}], "edges": [[0, 0]],
                     "connections": [{"from": 0, "to": 0, "input": port}]}
            with self.subTest(port=port), self.assertRaises(ValueError):
                validate_graph(graph)
        with self.assertRaises(ValueError):
            validate_graph({"nodes": [{"id": 0, "op": "A"}], "edges": [],
                            "connections": [{"from": 0, "to": 0, "input": 1}]})

    def test_skipped_strict_group_keeps_its_empty_match(self):
        for jobs in [1, 4]:
            process = self.invoke("-i", self.graph, "-r", "($B)?", "--jobs", jobs)
            self.assertEqual(process.returncode, 0, process.stderr)
            matches = json.loads(process.stdout)["matches"]
            self.assertEqual({tuple(m["starts"]) for m in matches}, {(0,), (9,), (42,)})
            self.assertTrue(all(m["node_ids"] == [] for m in matches))
            self.assertTrue(all(m["walks"][0]["quantifiers"][0]["count"] == 0 for m in matches))

    def test_directional_strict_nodes_in_public_reports(self):
        for extra in range(4):
            graph = {"nodes": [{"id": 42, "op": "A"}, {"id": 0, "op": "B", "name": "hub"},
                               {"id": 9, "op": "C"}, {"id": 7, "op": "Other"}],
                     "edges": [[42, 0], [0, 9]]}
            if extra & 1:
                graph["edges"].append([7, 0])
            if extra & 2:
                graph["edges"].append([0, 7])
            self.graph.write_text(json.dumps(graph), encoding="utf-8")
            for sides, key in enumerate(["B#", "$B", "B$", "$B$"]):
                query = "A->" + key + "->C->$"
                reference = None
                for jobs in [1, 4]:
                    with self.subTest(extra=extra, query=query, jobs=jobs):
                        process = self.invoke("-i", self.graph, "-r", query, "--jobs", jobs)
                        self.assertEqual(process.returncode, 0, process.stderr)
                        report = json.loads(process.stdout)
                        self.assertEqual(report["query"], query)
                        self.assertEqual(len(report["matches"]), int((sides & extra) == 0))
                        for match in report["matches"]:
                            self.assertEqual(match["node_ids"], [0, 9, 42])
                            self.assertEqual(match["walks"][0]["nodes"][1],
                                             {"id": 0, "name": "hub", "predicate": "B"})
                        if reference is not None:
                            self.assertEqual(report, reference)
                        reference = report

    def test_anchor_roots_start_every_walk_on_the_anchor(self):
        graph = {"nodes": [{"id": 42, "op": "A"}, {"id": 0, "op": "B", "name": "hub"},
                           {"id": 9, "op": "C"}, {"id": 7, "op": "D"}],
                 "edges": [[42, 0], [0, 9], [0, 7]]}
        self.graph.write_text(json.dumps(graph), encoding="utf-8")
        for query in ["A->B#->C", "A->B#;B#->C;B#->D", "C<-B#<-A"]:
            reference = None
            for jobs in [1, 4]:
                with self.subTest(query=query, jobs=jobs):
                    process = self.invoke("-i", self.graph, "-r", query, "--jobs", jobs)
                    self.assertEqual(process.returncode, 0, process.stderr)
                    report = json.loads(process.stdout)
                    self.assertEqual(len(report["matches"]), 1)
                    match = report["matches"][0]
                    self.assertEqual(set(match["starts"]), {0})
                    hub = {"id": 0, "name": "hub", "predicate": "B"}
                    self.assertTrue(all(walk["nodes"][0] == hub for walk in match["walks"]))
                    captured = {node["id"] for walk in match["walks"] for node in walk["nodes"]}
                    self.assertEqual(captured, set(match["node_ids"]))
                    if reference is not None:
                        self.assertEqual(report, reference)
                    reference = report
        # One start per anchor node: a budget of one tuple suffices.
        self.assertEqual(self.invoke("-i", self.graph, "-r", "A->B#;B#->C;B#->D", "--max-starts", 1).returncode, 0)

    def test_parallel_matches_preserve_names_and_quantifier_allocations(self):
        for query in ["A+->B", "(A?)+->B", "B<-A", "A+->A+->B", "Missing",
                      "A+->B->$", "A+->$", "A+->B->Missing?->$", "$<-B<-A"]:
            with self.subTest(query=query):
                serial = self.invoke("-i", self.graph, "-r", query)
                self.assertEqual(serial.returncode, 0, serial.stderr)
                for jobs in [2, 4, 16]:
                    parallel = self.invoke("-i", self.graph, "-r", query, "--jobs", jobs)
                    self.assertEqual(parallel.returncode, 0, parallel.stderr)
                    self.assertEqual(json.loads(parallel.stdout), json.loads(serial.stdout))

    def test_parallel_rejects_bad_jobs_and_preserves_existing_output_on_budget_error(self):
        output = self.folder / "existing.json"
        output.write_text("keep me", encoding="utf-8")
        for value in ["0", "-1", "257", "1.5"]:
            result = self.invoke("-i", self.graph, "-r", "A", "--jobs", value, "-o", output)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(output.read_text(encoding="utf-8"), "keep me")
        result = self.invoke("-i", self.graph, "-r", "A;B", "-j", 4,
                             "--max-starts", 1, "-o", output)
        self.assertEqual(result.returncode, 2)
        self.assertIn("starting combinations", result.stderr)
        self.assertEqual(output.read_text(encoding="utf-8"), "keep me")

    def test_conditional_node_example_runs_outside_repository(self):
        output = self.folder / "conditional report.json"
        process = subprocess.run([sys.executable, str(ROOT / "examples/matches/run_rules.py"),
                                  "--output", str(output)], cwd=self.folder,
                                 env={**os.environ, "PYTHONPATH": str(ROOT / "python")},
                                 capture_output=True, text=True, timeout=30)
        self.assertEqual(process.returncode, 0, process.stderr)
        report = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual([m["node_ids"] for m in report["matches"]], [[0, 42]])
        self.assertEqual(report["node_definitions"]["conv3x3"]["node_ids"], [42])

    def test_example_script_runs_outside_repository(self):
        output = self.folder / "example results"
        process = subprocess.run([sys.executable, str(ROOT / "examples/matches/run_matches.py"),
                                  "--output-dir", str(output)], cwd=self.folder,
                                 env={**os.environ, "PYTHONPATH": str(ROOT / "python")},
                                 capture_output=True, text=True, timeout=30)
        self.assertEqual(process.returncode, 0, process.stderr)
        reports = {path.stem: json.loads(path.read_text(encoding="utf-8")) for path in output.glob("*.json")}
        self.assertEqual(set(reports), {"convolutions", "arithmetic", "no-match"})
        self.assertEqual(reports["no-match"]["matches"], [])
        self.assertEqual(reports["arithmetic"]["matches"][0]["node_ids"], [0, 9, 17, 42])
        full = next(m for m in reports["convolutions"]["matches"] if m["starts"] == [42])
        self.assertEqual(full["walks"][0]["quantifiers"][0]["count"], 2)

    def test_bundled_node_library_maps_each_format_without_false_positives(self):
        from corgireg_cli.cli import find_engine
        from corgireg_cli.graph import encode_request
        from corgireg_cli.node_library import library_arguments, load_definitions
        library = [ROOT / "examples/library/nodes"]
        definitions = load_definitions(library)
        expected = {"conv": ("Conv", "convolution"), "negate": ("Neg", "negate"),
                    "abs": ("Abs", "abs"), "add": ("Add", "add"),
                    "multiply": ("Mul", "multiply"), "subtract": ("Sub", "subtract"),
                    "divide": ("Div", "divide"), "exponential": ("Exp", "exponential"),
                    "log": ("Log", "log"), "sqrt": ("Sqrt", "sqrt"), "tanh": ("Tanh", "tanh")}
        self.assertEqual(set(definitions), set(expected))
        self.assertEqual(definitions["conv"]["source"], str((library[0] / "conv.json").resolve()))
        for name, (onnx, hlo) in expected.items():
            for format_name, op in [("onnx", onnx), ("stablehlo", "stablehlo." + hlo), ("json", onnx)]:
                with self.subTest(name=name, format=format_name):
                    graph = validate_graph({"nodes": [{"id": 42, "op": op},
                                                       {"id": 9, "op": "Unrelated"}], "edges": []})
                    process = subprocess.run([str(find_engine()), *library_arguments(library, format_name)],
                                             input=encode_request(graph, name, 100, {}, {}),
                                             capture_output=True, text=True, encoding="utf-8", timeout=20)
                    self.assertEqual(process.returncode, 0, process.stderr)
                    report = json.loads(process.stdout)["node_definitions"]
                    self.assertEqual(report[name]["node_ids"], [42])
                    self.assertTrue(all(value["node_ids"] == [] for key, value in report.items() if key != name))

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="corgireg-test-", dir=ROOT / "build")
        self.addCleanup(self.directory.cleanup)
        self.folder = Path(self.directory.name)
        self.graph = self.folder / "source graph.json"
        self.graph.write_text(json.dumps({"nodes": [
            {"id": 42, "op": "A", "name": 'first "node"\nline'},
            {"id": 0, "op": "A", "name": "zéro"}, {"id": 9, "op": "B"}],
            "edges": [[42, 0], [0, 9]]}), encoding="utf-8")

    def invoke(self, *args):
        env = {**os.environ, "PYTHONPATH": str(ROOT / "python"), "PYTHONIOENCODING": "utf-8"}
        return subprocess.run([sys.executable, "-m", "corgireg_cli", *map(str, args)],
                              capture_output=True, text=True, encoding="utf-8", env=env, timeout=20)

    def test_inline_and_file_patterns_return_same_captures(self):
        direct = self.invoke("-i", self.graph, "-r", "A+->B")
        self.assertEqual(direct.returncode, 0, direct.stderr)
        result = json.loads(direct.stdout)
        full = [m for m in result["matches"] if m["starts"] == [42]][0]
        self.assertEqual(full["node_ids"], [0, 9, 42])
        self.assertEqual(full["walks"][0]["quantifiers"][0]["count"], 2)
        self.assertEqual(full["walks"][0]["nodes"][1]["name"], "zéro")
        regex = self.folder / "pattern.txt"
        regex.write_text("A+->\nB\n", encoding="utf-8-sig")
        output = self.folder / "result.json"
        file_run = self.invoke("-i", self.graph, "-R", regex, "-o", output)
        self.assertEqual(file_run.returncode, 0, file_run.stderr)
        self.assertEqual(file_run.stdout, "")
        file_result = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(file_result["matches"], result["matches"])
        self.assertEqual(file_result["query"], "A+->\nB\n")

    def test_errors_preserve_existing_output(self):
        output = self.folder / "result.json"
        output.write_text("keep", encoding="utf-8")
        for arguments in [("-r", "A->"), ("-r", " "), ("-r", "A", "--max-starts", "1"),
                          ("-r", "A", "--format", "unknown")]:
            run = self.invoke("-i", self.graph, "-o", output, *arguments)
            self.assertEqual(run.returncode, 2, run.stderr)
            self.assertEqual(output.read_text(encoding="utf-8"), "keep")
        before = self.graph.read_bytes()
        self.assertEqual(self.invoke("-i", self.graph, "-r", "A", "-o", self.graph).returncode, 2)
        self.assertEqual(self.graph.read_bytes(), before)

    def test_no_match_is_a_valid_report(self):
        run = self.invoke("-i", self.graph, "-r", "MissingOp")
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(json.loads(run.stdout)["matches"], [])

    def test_required_exclusive_options_and_missing_files(self):
        self.assertEqual(self.invoke("--help").returncode, 0)
        self.assertEqual(self.invoke("-i", self.graph).returncode, 2)
        self.assertEqual(self.invoke("-i", self.graph, "-r", "A", "-R", "missing").returncode, 2)
        self.assertEqual(self.invoke("-i", "missing.json", "-r", "A").returncode, 2)
        self.assertEqual(self.invoke("-i", self.graph, "-R", "missing").returncode, 2)

    def test_graph_validation_rejects_corrupt_identity(self):
        for graph in [{}, {"nodes": [{"id": True, "op": "A"}], "edges": []},
                      {"nodes": [{"id": 0, "op": "A"}], "edges": [[0, 1]]},
                      {"nodes": [{"id": 0, "op": "A"}, {"id": 0, "op": "B"}], "edges": []},
                      {"nodes": [{"id": 0, "op": "a.b"}, {"id": 1, "op": "a_b"}], "edges": []}]:
            with self.subTest(graph=graph), self.assertRaises(ValueError):
                validate_graph(graph)

    def definition(self, name="conv", formats=None):
        path = self.folder / (name + ".json")
        path.write_text(json.dumps({"schema_version": 1, "name": name, "description": "Portable predicate",
                                    "formats": formats or {"json": {"op_types": ["A"]}}}), encoding="utf-8")
        return path

    def test_native_node_rules_filter_memberships_and_preserve_traces(self):
        graph = json.loads(self.graph.read_text(encoding="utf-8"))
        for node in graph["nodes"]:
            node["attributes"] = {"kernel": [3, 3], "group": 1, "nullable": None}
        self.graph.write_text(json.dumps(graph), encoding="utf-8")
        path = self.definition()
        definition = json.loads(path.read_text(encoding="utf-8"))
        definition["schema_version"] = 2
        definition["formats"]["json"]["where"] = {"all": [
            {"path": ["attributes", "kernel"], "eq": [3, 3]},
            {"path": ["attributes", "group"], "gte": 1},
            {"path": ["attributes", "nullable"], "exists": True},
            {"keys": ["attributes"], "regex": "debug_.*", "count": {"eq": 0}}]}
        path.write_text(json.dumps(definition), encoding="utf-8")
        result = self.invoke("-i", self.graph, "-r", "conv+->B", "--node-library", path)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        full = next(m for m in report["matches"] if m["starts"] == [42])
        self.assertEqual(full["walks"][0]["quantifiers"][0]["count"], 2)
        self.assertEqual(report["node_definitions"]["conv"]["node_ids"], [0, 42])
        parallel = self.invoke("-i", self.graph, "-r", "conv+->B", "--node-library", path, "-j", 4)
        self.assertEqual(parallel.returncode, 0, parallel.stderr)
        self.assertEqual(json.loads(parallel.stdout), report)
        graph["nodes"][0]["attributes"]["debug_trace"] = False
        self.graph.write_text(json.dumps(graph), encoding="utf-8")
        result = self.invoke("-i", self.graph, "-r", "conv+->B", "--node-library", path)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["node_definitions"]["conv"]["node_ids"], [0])
        self.assertFalse(any(m["starts"] == [42] for m in report["matches"]))

    def test_invalid_native_rules_fail_even_without_candidates_and_preserve_output(self):
        path = self.definition(formats={"json": {"op_types": ["MissingOp"]}})
        definition = json.loads(path.read_text(encoding="utf-8"))
        definition["schema_version"] = 2
        output = self.folder / "keep.json"
        output.write_text("keep", encoding="utf-8")
        for rule in [{"path": ["name"], "regex": "["}, {"path": ["x"], "exists": 1},
                     {"path": ["x"], "gt": "3"}, {"unknown": True}]:
            definition["formats"]["json"]["where"] = rule
            path.write_text(json.dumps(definition), encoding="utf-8")
            result = self.invoke("-i", self.graph, "-r", "conv", "--node-library", path, "-o", output)
            self.assertEqual(result.returncode, 2)
            self.assertIn("where", result.stderr)
            self.assertIn(str(path), result.stderr)
            self.assertEqual(output.read_text(encoding="utf-8"), "keep")

    def test_node_library_preserves_pattern_and_original_source_names(self):
        definition = self.definition()
        result = self.invoke("-i", self.graph, "-r", "conv+->B", "--node-library", definition)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        match = next(m for m in report["matches"] if m["starts"] == [42])
        self.assertEqual(report["query"], "conv+->B")
        self.assertEqual(match["walks"][0]["quantifiers"][0]["count"], 2)
        self.assertEqual(match["walks"][0]["nodes"][0]["predicate"], "conv")
        self.assertEqual(match["walks"][0]["nodes"][1]["name"], "zéro")
        self.assertEqual(report["node_definitions"]["conv"]["node_ids"], [0, 42])
        self.assertEqual(report["graph"]["nodes"][0]["op"], "A")

    def test_multiple_definitions_can_match_one_source_type(self):
        conv, layer = self.definition(), self.definition("layer")
        result = self.invoke("-i", self.graph, "-r", "conv->layer->B", "--node-library", conv,
                             "--node-library", layer)
        self.assertEqual(result.returncode, 0, result.stderr)
        nodes = json.loads(result.stdout)["matches"][0]["walks"][0]["nodes"]
        self.assertEqual([node["predicate"] for node in nodes], ["conv", "layer", "B"])
        # Explicit definitions take precedence over raw operation predicate names.
        override = self.definition("A", {"json": {"op_types": ["B"]}})
        result = self.invoke("-i", self.graph, "-r", "A", "--node-library", override)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["matches"][0]["node_ids"], [9])

    def test_missing_mapping_differs_from_no_matching_operations(self):
        definition = self.definition(formats={"onnx": {"op_types": ["Conv"]}})
        result = self.invoke("-i", self.graph, "-r", "conv+", "--node-library", definition)
        self.assertEqual(result.returncode, 2)
        self.assertIn("No definition for predicate 'conv'", result.stderr)
        self.assertEqual(self.invoke("-i", self.graph, "-r", "A", "--node-library", definition).returncode, 0)
        definition = self.definition(formats={"json": {"op_types": ["MissingOp"]}})
        result = self.invoke("-i", self.graph, "-r", "conv+", "--node-library", definition)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["matches"], [])

    def test_invalid_libraries_and_overwrite_are_rejected(self):
        definition = self.definition()
        before = definition.read_bytes()
        result = self.invoke("-i", self.graph, "-r", "conv", "--node-library", definition, "-o", definition)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(definition.read_bytes(), before)
        self.assertEqual(self.invoke("-i", self.graph, "-r", "conv", "--node-library", definition,
                                     "--node-library", definition).returncode, 2)
        data = json.loads(before)
        data["formats"]["json"]["attributes"] = {"ignored": True}
        definition.write_text(json.dumps(data), encoding="utf-8")
        result = self.invoke("-i", self.graph, "-r", "conv", "--node-library", definition)
        self.assertEqual(result.returncode, 2)
        self.assertIn("Only op_types", result.stderr)

    def test_definition_directory_is_loaded_deterministically(self):
        directory = self.folder / "nodes"
        directory.mkdir()
        self.definition().rename(directory / "conv.json")
        self.definition("layer").rename(directory / "layer.json")
        result = self.invoke("-i", self.graph, "-r", "conv->layer->B", "--node-library", directory)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(list(json.loads(result.stdout)["node_definitions"]), ["conv", "layer"])

    def test_definition_directories_are_not_recursive_and_ignore_other_files(self):
        directory = self.folder / "nodes"
        (directory / "nested").mkdir(parents=True)
        self.definition().rename(directory / "conv.json")
        self.definition("hidden").rename(directory / "nested" / "hidden.json")
        (directory / "notes.txt").write_text("not a definition", encoding="utf-8")
        result = self.invoke("-i", self.graph, "-r", "conv", "--node-library", directory)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(list(json.loads(result.stdout)["node_definitions"]), ["conv"])
        result = self.invoke("-i", self.graph, "-r", "conv", "--node-library", directory / "nested")
        self.assertEqual(result.returncode, 0, result.stderr)
        empty = self.folder / "empty"
        empty.mkdir()
        result = self.invoke("-i", self.graph, "-r", "A", "--node-library", empty)
        self.assertEqual(result.returncode, 2)
        self.assertIn("No node definitions found", result.stderr)
        result = self.invoke("-i", self.graph, "-r", "A", "--node-library", directory / "notes.txt")
        self.assertEqual(result.returncode, 2)
        self.assertIn("must be JSON files", result.stderr)

    def test_node_definition_reports_identify_source_rule_and_format(self):
        path = self.definition(formats={"json": {"op_types": ["A", "B"]}, "onnx": {"op_types": ["Conv"]}})
        missing = self.definition("other", {"onnx": {"op_types": ["Conv"]}})
        result = self.invoke("-i", self.graph, "-r", "conv", "--node-library", path, "--node-library", missing)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)["node_definitions"]
        self.assertEqual(report["conv"], {"description": "Portable predicate", "source": str(path.resolve()),
                                          "openmath": None, "format": "json", "rule": {"op_types": ["A", "B"]},
                                          "node_ids": [0, 9, 42]})
        self.assertEqual(report["other"], {"description": "Portable predicate", "source": str(missing.resolve()),
                                           "openmath": None, "format": "json", "rule": None, "node_ids": None})

    def test_invalid_definition_documents_are_rejected(self):
        output = self.folder / "keep.json"
        output.write_text("keep", encoding="utf-8")
        base = {"schema_version": 1, "name": "conv", "formats": {"json": {"op_types": ["A"]}}}
        symbol = {"cd": "arith1", "name": "plus"}
        cases = [({**base, "schema_version": 5}, "Unsupported node definition schema"),
                 ({**base, "openmath": symbol}, "openmath requires schema_version 3"),
                 ({**base, "schema_version": 3, "openmath": {**symbol, "cd": "arith 1"}}, "openmath cd must match"),
                 ({**base, "schema_version": True}, "Unsupported node definition schema"),
                 ({**base, "schema_version": 1.0}, "Unsupported node definition schema"),
                 ({**base, "name": "a-b"}, "Invalid predicate name"),
                 ({**base, "name": ""}, "Invalid predicate name"),
                 ({**base, "description": 3}, "Invalid description"),
                 ({**base, "formats": {}}, "requires format mappings"),
                 ({**base, "extra": 1}, "Invalid node definition fields"),
                 ({**base, "formats": {"json": {"op_types": []}}}, "op_types"),
                 ({**base, "formats": {"json": {"op_types": ["A", 3]}}}, "op_types"),
                 ({**base, "formats": {"json": {"op_types": ["A"], "where": {"path": ["op"], "eq": "A"}}}},
                  "Only op_types"),
                 ({**base, "schema_version": 2, "formats": {"json": {"op_types": ["A"], "where": []}}},
                  "where must be a rule object"),
                 ([base], "Invalid node definition fields")]
        path = self.folder / "conv.json"
        for document, message in cases:
            with self.subTest(document=document):
                path.write_text(json.dumps(document), encoding="utf-8")
                result = self.invoke("-i", self.graph, "-r", "A", "--node-library", path, "-o", output)
                self.assertEqual(result.returncode, 2)
                self.assertIn(message, result.stderr)
                self.assertEqual(output.read_text(encoding="utf-8"), "keep")
        path.write_text("{", encoding="utf-8")
        self.assertEqual(self.invoke("-i", self.graph, "-r", "A", "--node-library", path).returncode, 2)
        first, second = self.folder / "a", self.folder / "b"
        first.mkdir(), second.mkdir()
        self.definition().rename(first / "conv.json")
        self.definition().rename(second / "conv.json")
        result = self.invoke("-i", self.graph, "-r", "A", "--node-library", first, "--node-library", second)
        self.assertEqual(result.returncode, 2)
        self.assertIn("Duplicate node definition: conv", result.stderr)


if __name__ == "__main__":
    unittest.main()
