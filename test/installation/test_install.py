import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from installation import install as installer


class InstallationTests(unittest.TestCase):
    def test_help_and_invalid_job_count_without_installation(self):
        script = ROOT / "installation/install.py"
        self.assertEqual(subprocess.run([sys.executable, script, "--help"], capture_output=True).returncode, 0)
        result = subprocess.run([sys.executable, script, "--jobs", "0"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("must be positive", result.stderr)

    def test_non_environment_directory_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            marker = folder / "important.txt"
            marker.write_text("keep", encoding="utf-8")
            args = argparse.Namespace(venv=folder, build_dir=folder / "build", formats="json", jobs=1, generator=None)
            with patch.object(installer.shutil, "which", return_value="tool"), self.assertRaisesRegex(ValueError, "non-virtual"):
                installer.install(args)
            self.assertEqual(marker.read_text(encoding="utf-8"), "keep")

    def test_failed_command_stops_installation(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            env = folder / "venv with spaces"
            python = env / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
            python.parent.mkdir(parents=True)
            python.touch()
            (env / "pyvenv.cfg").touch()
            args = argparse.Namespace(venv=env, build_dir=folder / "build", formats="json", jobs=1, generator=None)
            with patch.object(installer.shutil, "which", return_value="tool"), \
                 patch.object(installer, "run", side_effect=subprocess.CalledProcessError(3, "python")) as run:
                with self.assertRaises(subprocess.CalledProcessError):
                    installer.install(args)
                self.assertEqual(run.call_count, 1)
                self.assertFalse(args.build_dir.exists())

    def test_environment_worker_wins_over_unrelated_path_executable(self):
        sys.path.insert(0, str(ROOT / "python"))
        from corgireg_cli.cli import find_engine
        with tempfile.TemporaryDirectory() as directory:
            engine = Path(directory) / "bin" / ("corgireg-match.exe" if os.name == "nt" else "corgireg-match")
            engine.parent.mkdir()
            engine.touch()
            with patch.dict(os.environ, {}, clear=True), patch.object(sys, "prefix", directory), \
                 patch("corgireg_cli.cli.shutil.which", return_value="unrelated"):
                self.assertEqual(find_engine(), engine.resolve())
                with self.assertRaises(ValueError):
                    find_engine(Path(directory) / "missing")

    def test_no_discovered_tests_is_an_installation_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            env = folder / "venv"
            python = env / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
            python.parent.mkdir(parents=True)
            python.touch()
            (env / "pyvenv.cfg").touch()
            engine = env / "bin" / ("corgireg-match.exe" if os.name == "nt" else "corgireg-match")
            engine.parent.mkdir(exist_ok=True)
            engine.touch()
            args = argparse.Namespace(venv=env, build_dir=folder / "build", formats="json", jobs=1, generator=None)
            result = subprocess.CompletedProcess([], 0, stdout='{"tests": []}')
            with patch.object(installer.shutil, "which", return_value="tool"), \
                 patch.object(installer, "run", return_value=result), \
                 self.assertRaisesRegex(ValueError, "No tests were discovered"):
                installer.install(args)


if __name__ == "__main__":
    unittest.main()
