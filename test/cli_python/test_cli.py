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


if __name__ == "__main__":
    unittest.main()
