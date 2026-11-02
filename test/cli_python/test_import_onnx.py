import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import onnx
from onnx import helper, TensorProto

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python"))
from corgireg_cli.import_onnx import load_onnx


class OnnxTests(unittest.TestCase):
    def test_inline_keys_use_json_defaults_and_foreign_ir_aliases(self):
        self.save([helper.make_node("Conv", ["x", "w"], ["a"], kernel_shape=[3, 3],
                                    group=1, strides=[1, 1]),
                   helper.make_node("Relu", ["a"], ["z"])], inputs=("x", "w"))
        library = ROOT / "examples/library/inline-nodes"
        for query, expected in [("conv(kernel_shape=[3,3],group=1)->Relu", True),
                                ("conv(feature_group_count=1,window_strides=[1,1])->Relu", True),
                                ("conv(kernel_shape=[5,5])->Relu", False),
                                ("conv(group=2)->Relu", False)]:
            with self.subTest(query=query):
                process = subprocess.run([sys.executable, "-m", "corgireg_cli", "-i", str(self.path),
                    "--node-library", str(library), "-r", query],
                    env={**os.environ, "PYTHONPATH": str(ROOT / "python")}, capture_output=True, text=True, timeout=20)
                self.assertEqual(process.returncode, 0, process.stderr)
                matches = json.loads(process.stdout)["matches"]
                self.assertEqual(bool(matches), expected)
                if expected:
                    self.assertEqual(matches[0]["node_ids"], [0, 1])

    def test_repeated_operand_retains_two_input_connections(self):
        self.save([helper.make_node("Identity", ["x"], ["y"]),
                   helper.make_node("Sub", ["y", "y"], ["z"])])
        graph = load_onnx(self.path)
        self.assertEqual(graph["edges"], [(0, 1)])
        self.assertEqual(graph["connections"], [
            {"from": 0, "to": 1, "input": 1, "output": 1},
            {"from": 0, "to": 1, "input": 2, "output": 1}])

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(dir=ROOT / "build")
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "network.onnx"

    def save(self, nodes, inputs=("x",), outputs=("z",), initializers=()):
        value = lambda name: helper.make_tensor_value_info(name, TensorProto.FLOAT, [1])
        graph = helper.make_graph(nodes, "fixture", [value(n) for n in inputs], [value(n) for n in outputs],
                                  initializer=list(initializers))
        onnx.save_model(helper.make_model(graph), self.path)

    def test_real_model_roundtrip_cli_preserves_operation_indices(self):
        self.save([helper.make_node("Relu", ["x"], ["a"], name="first"),
                   helper.make_node("Relu", ["a"], ["b"], name="second"),
                   helper.make_node("Neg", ["b"], ["z"], name="last")])
        env = {**os.environ, "PYTHONPATH": str(ROOT / "python")}
        result = subprocess.run([sys.executable, "-m", "corgireg_cli", "-i", str(self.path),
                                 "-r", "Relu+->Neg"], env=env, capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        match = next(m for m in report["matches"] if m["starts"] == [0])
        self.assertEqual(match["node_ids"], [0, 1, 2])
        self.assertEqual(match["walks"][0]["quantifiers"][0]["count"], 2)
        self.assertEqual(match["walks"][0]["nodes"][0]["name"], "first")
        self.assertEqual(report["graph"]["nodes"][1]["outputs"], ["b"])

    def test_shared_conv_library_matches_real_convolutions(self):
        value = lambda name, shape: helper.make_tensor_value_info(name, TensorProto.FLOAT, shape)
        graph = helper.make_graph([
            helper.make_node("Conv", ["x", "w"], ["a"], name="conv_first"),
            helper.make_node("Conv", ["a", "w"], ["z"], name="conv_second")],
            "convolutions", [value("x", [1, 1, 3, 3]), value("w", [1, 1, 1, 1])],
            [value("z", [1, 1, 3, 3])])
        model = helper.make_model(graph)
        onnx.checker.check_model(model)
        onnx.save_model(model, self.path)
        process = subprocess.run([sys.executable, "-m", "corgireg_cli", "-i", str(self.path),
                                  "--node-library", str(ROOT / "examples/library/nodes"),
                                  "-R", str(ROOT / "examples/library/patterns/conv-chain.txt")],
                                 env={**os.environ, "PYTHONPATH": str(ROOT / "python")},
                                 capture_output=True, text=True, timeout=20)
        self.assertEqual(process.returncode, 0, process.stderr)
        report = json.loads(process.stdout)
        match = next(m for m in report["matches"] if m["starts"] == [0])
        self.assertEqual(match["node_ids"], [0, 1])
        self.assertEqual(match["walks"][0]["quantifiers"][0]["count"], 2)
        self.assertEqual(report["node_definitions"]["conv"]["node_ids"], [0, 1])
        self.assertEqual(report["graph"]["nodes"][0]["name"], "conv_first")
        self.assertEqual(report["node_definitions"]["negate"]["node_ids"], [])

    def test_inputs_initializers_and_duplicate_operands_are_boundaries(self):
        weight = helper.make_tensor("w", TensorProto.FLOAT, [1], [1.0])
        self.save([helper.make_node("Add", ["x", "w"], ["a"]),
                   helper.make_node("Add", ["a", "a"], ["z"])], initializers=[weight])
        graph = load_onnx(self.path)
        self.assertEqual(len(graph["nodes"]), 2)
        self.assertEqual(graph["edges"], [(0, 1)])

    def test_declared_attributes_and_absent_optional_inputs_reach_node_test(self):
        value = lambda name, shape: helper.make_tensor_value_info(name, TensorProto.FLOAT, shape)
        graph = helper.make_graph([helper.make_node("Conv", ["x", "w", ""], ["z"],
                                                    name="encoder/conv_0", kernel_shape=[3, 3])],
                                  "attributes", [value("x", [1, 1, 5, 5]), value("w", [1, 1, 3, 3])],
                                  [value("z", [1, 1, 3, 3])])
        model = helper.make_model(graph)
        onnx.checker.check_model(model)
        onnx.save_model(model, self.path)
        node = load_onnx(self.path)["nodes"][0]
        self.assertEqual(node["attributes"], {"kernel_shape": [3, 3]})
        self.assertEqual(node["inputs"], ["x", "w", ""])
        definition = {"schema_version": 2, "name": "conv3", "formats": {"onnx": {
            "op_types": ["Conv"], "where": {"all": [
                {"path": ["attributes", "kernel_shape"], "eq": [3, 3]},
                {"path": ["attributes", "group"], "exists": False},
                {"path": ["inputs", 2], "exists": False},
                {"path": ["name"], "regex": "encoder/conv_[0-9]+"}]}}}}
        library = self.path.with_suffix(".json")
        for expected, bias_present in [([0], False), ([], True)]:
            definition["formats"]["onnx"]["where"]["all"][2]["exists"] = bias_present
            library.write_text(json.dumps(definition), encoding="utf-8")
            result = subprocess.run([sys.executable, "-m", "corgireg_cli", "-i", str(self.path),
                                     "--node-library", str(library), "-r", "conv3"],
                                    env={**os.environ, "PYTHONPATH": str(ROOT / "python")},
                                    capture_output=True, text=True, timeout=20)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads(result.stdout)
            self.assertEqual(report["node_definitions"]["conv3"]["node_ids"], expected)
            self.assertEqual([m["node_ids"][0] for m in report["matches"]], expected)

    def test_invalid_values_and_nested_graphs_are_rejected(self):
        for nodes in [[helper.make_node("Relu", ["missing"], ["z"])],
                      [helper.make_node("Relu", ["x"], ["z"]), helper.make_node("Relu", ["x"], ["z"])]]:
            self.save(nodes)
            with self.assertRaises(ValueError):
                load_onnx(self.path)
        branch = helper.make_graph([], "branch", [], [])
        self.save([helper.make_node("If", ["x"], ["z"], then_branch=branch, else_branch=branch)])
        with self.assertRaisesRegex(ValueError, "Nested ONNX"):
            load_onnx(self.path)


if __name__ == "__main__":
    unittest.main()
