import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python"))
from corgireg_cli.import_stablehlo import load_stablehlo


MODULE = """module {
  func.func @main(%arg0: tensor<2xf32>) -> tensor<2xf32> {
    %0 = stablehlo.negate %arg0 : tensor<2xf32>
    %1 = stablehlo.negate %0 : tensor<2xf32>
    %2 = stablehlo.abs %1 : tensor<2xf32>
    return %2 : tensor<2xf32>
  }
}
"""


class StablehloTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(dir=ROOT / "build")
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "network.mlir"
        self.path.write_text(MODULE, encoding="utf-8")

    def test_real_mlir_cli_recovers_operation_indices_and_repetitions(self):
        env = {**os.environ, "PYTHONPATH": str(ROOT / "python")}
        process = subprocess.run([sys.executable, "-m", "corgireg_cli", "-i", str(self.path),
                                  "-r", "stablehlo_negate+->stablehlo_abs"], env=env,
                                 capture_output=True, text=True, timeout=20)
        self.assertEqual(process.returncode, 0, process.stderr)
        result = json.loads(process.stdout)
        match = next(m for m in result["matches"] if m["starts"] == [0])
        self.assertEqual(match["node_ids"], [0, 1, 2])
        self.assertEqual(match["walks"][0]["quantifiers"][0]["count"], 2)
        self.assertEqual(result["graph"]["edges"], [[0, 1], [1, 2]])
        self.assertEqual(result["graph"]["nodes"][0]["function"], "main")
        self.assertEqual(result["graph"]["nodes"][0]["op"], "stablehlo.negate")

    def test_shared_conv_library_matches_real_convolutions(self):
        process = subprocess.run([sys.executable, "-m", "corgireg_cli", "-i",
                                  str(ROOT / "examples/library/conv.mlir"),
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
        self.assertEqual(report["graph"]["nodes"][0]["op"], "stablehlo.convolution")
        self.assertEqual(report["node_definitions"]["negate"]["node_ids"], [])

    def test_function_selection_is_explicit_when_ambiguous(self):
        functions = MODULE.removeprefix("module {").rsplit("}", 1)[0]
        self.path.write_text("module {" + functions.replace("@main", "@left") +
                             functions.replace("@main", "@right") + "}", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "--function"):
            load_stablehlo(self.path)
        graph = load_stablehlo(self.path, "right")
        self.assertTrue(all(node["function"] == "right" for node in graph["nodes"]))
        with self.assertRaisesRegex(ValueError, "not found"):
            load_stablehlo(self.path, "missing")

    def test_duplicate_operands_do_not_duplicate_edges(self):
        self.path.write_text(MODULE.replace("stablehlo.abs %1", "stablehlo.add %1, %1"), encoding="utf-8")
        self.assertEqual(load_stablehlo(self.path)["edges"], [(0, 1), (1, 2)])

    def test_invalid_mlir_and_nested_regions_are_rejected(self):
        self.path.write_text("not a module", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Cannot parse"):
            load_stablehlo(self.path)
        self.path.write_text('''module {
          func.func @main(%arg0: tensor<2xf32>) -> tensor<f32> {
            %init = stablehlo.constant dense<0.0> : tensor<f32>
            %r = stablehlo.reduce(%arg0 init: %init) applies stablehlo.add across dimensions = [0]
                 : (tensor<2xf32>, tensor<f32>) -> tensor<f32>
            return %r : tensor<f32>
          }
        }''', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "nested region"):
            load_stablehlo(self.path)


if __name__ == "__main__":
    unittest.main()
