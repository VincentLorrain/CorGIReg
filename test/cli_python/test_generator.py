import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class GeneratorTests(unittest.TestCase):
    def generate(self, *args):
        return subprocess.run([os.environ["CORGIREG_GENERATOR"], *map(str, args)],
                              capture_output=True, text=True, timeout=20)

    def test_file_values_generate_numbered_graph_that_matches(self):
        result = self.generate("-r", "(A+->B)+->C*->D?", "-v", ROOT / "examples/generation/nested.values")
        self.assertEqual(result.returncode, 0, result.stderr)
        graph = json.loads(result.stdout)
        self.assertEqual([n["op"] for n in graph["nodes"]], list("AAABABD"))
        self.assertEqual(graph["edges"], [[i, i + 1] for i in range(6)])
        self.assertTrue(all(c["input"] == 1 for c in graph["connections"]))
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "graph.json"
            path.write_text(result.stdout, encoding="utf-8")
            process = subprocess.run([sys.executable, "-m", "corgireg_cli", "-i", str(path),
                                      "-r", "A+-1->B+-1->A+-1->B-1->D"],
                                     env={**os.environ, "PYTHONPATH": str(ROOT / "python")},
                                     capture_output=True, text=True, timeout=20)
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertTrue(any(m["node_ids"] == list(range(7)) for m in json.loads(process.stdout)["matches"]))

    def test_aliases_random_seeds_and_fixed_ports(self):
        fixed = self.generate("-r", "X#-1->Sub#;X#-2->Sub#")
        self.assertEqual(fixed.returncode, 0, fixed.stderr)
        self.assertEqual([c["input"] for c in json.loads(fixed.stdout)["connections"]], [1, 2])
        plain = self.generate("-r", "(A->Sum#)+", "--random", "--seed", 42, "--max-loops", 3)
        alias = self.generate("-r", "(A-*->Sum#)+", "--random", "--seed", 42, "--max-loops", 3)
        self.assertEqual(plain.returncode, 0, plain.stderr)
        self.assertEqual(alias.returncode, 0, alias.stderr)
        self.assertEqual(json.loads(plain.stdout), json.loads(alias.stdout))

    def test_errors_do_not_replace_existing_output(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "keep.json"
            output.write_text("keep", encoding="utf-8")
            for args in [("-r", "A+"), ("-r", "A-0->B"), ("-r", "A", "--seed", "-1"),
                         ("-r", "A", "-v", "missing.values")]:
                result = self.generate(*args, "-o", output)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(output.read_text(encoding="utf-8"), "keep")


if __name__ == "__main__":
    unittest.main()
