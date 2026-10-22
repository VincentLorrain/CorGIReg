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

    def test_inputs_initializers_and_duplicate_operands_are_boundaries(self):
        weight = helper.make_tensor("w", TensorProto.FLOAT, [1], [1.0])
        self.save([helper.make_node("Add", ["x", "w"], ["a"]),
                   helper.make_node("Add", ["a", "a"], ["z"])], initializers=[weight])
        graph = load_onnx(self.path)
        self.assertEqual(len(graph["nodes"]), 2)
        self.assertEqual(graph["edges"], [(0, 1)])

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
