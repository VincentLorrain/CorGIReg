import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from benchmarks.track import compare_reports, summary, write_new


def report(samples):
    return {"schema_version": 1, "suite_sha256": "suite", "machine": {"name": "local"},
            "build": {"configuration": "Release"}, "sampling": {"samples": len(samples)},
            "cases": [{"name": "chain", "graph_nodes": 16, "start_arity": 1,
                       "expected_result": 1, "samples_ns": samples}]}


class ReportTests(unittest.TestCase):
    def test_statistics_preserve_outliers_without_using_them_as_median(self):
        values = summary([100, 110, 10000, 90, 100])
        self.assertEqual(values["median_ns"], 100)
        self.assertEqual(values["p95_ns"], 10000)
        self.assertEqual(values["mad_ns"], 10)
        for samples in [[], [0], [-1], [float("nan")], [float("inf")], [True]]:
            with self.assertRaises(ValueError):
                summary(samples)

    def test_direction_and_threshold(self):
        baseline = report([100, 100, 100])
        for duration, expected in [(150, "slower"), (50, "faster"), (105, "within_threshold")]:
            comparison = compare_reports(baseline, report([duration] * 3))
            self.assertEqual(comparison["changes"][0]["status"], expected)
            self.assertEqual(bool(comparison["regressions"]), expected == "slower")
        with self.assertRaises(ValueError):
            compare_reports(baseline, baseline, -1)

    def test_incompatible_reports_are_rejected(self):
        baseline = report([100] * 3)
        for key in ("suite_sha256", "machine", "build", "sampling"):
            changed = copy.deepcopy(baseline)
            changed[key] = "different"
            with self.subTest(key=key), self.assertRaises(ValueError):
                compare_reports(baseline, changed)
        changed = copy.deepcopy(baseline)
        changed["cases"][0]["graph_nodes"] = 32
        with self.assertRaises(ValueError):
            compare_reports(baseline, changed)
        changed = copy.deepcopy(baseline)
        changed["cases"].append(changed["cases"][0])
        with self.assertRaises(ValueError):
            compare_reports(baseline, changed)

    def test_reports_are_immutable_and_cli_can_gate_regressions(self):
        with tempfile.TemporaryDirectory() as directory:
            old, new = Path(directory) / "old.json", Path(directory) / "new.json"
            write_new(old, report([100] * 3))
            write_new(new, report([150] * 3))
            with self.assertRaises(FileExistsError):
                write_new(old, report([200] * 3))
            self.assertEqual(json.loads(old.read_text())["cases"][0]["samples_ns"], [100] * 3)
            command = [sys.executable, str(ROOT / "benchmarks/track.py"), "compare", str(old), str(new)]
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(subprocess.run(command + ["--fail-on-regression"], capture_output=True).returncode, 2)


if __name__ == "__main__":
    unittest.main()
