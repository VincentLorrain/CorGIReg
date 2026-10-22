"""Run portable JSON matching examples and check their expected captures."""
import argparse
import json
from pathlib import Path
import subprocess
import sys


HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--engine", type=Path, help="Optional native worker override")
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    cases = [("convolutions", "convolutions", [42, 0, 9]),
             ("arithmetic", "arithmetic", [42, 0, 9, 17]),
             ("no-match", "convolutions", [])]
    for name, graph, expected in cases:
        output = args.output_dir / (name + ".json")
        command = [sys.executable, "-m", "corgireg_cli",
                   "-i", str(HERE / "graphs" / (graph + ".json")),
                   "--node-library", str(HERE.parent / "library/nodes"),
                   "-R", str(HERE / "patterns" / (name + ".txt")), "-o", str(output)]
        if args.engine:
            command.extend(["--engine", str(args.engine)])
        subprocess.run(command, check=True, timeout=90)
        report = json.loads(output.read_text(encoding="utf-8"))
        if not expected:
            if report["matches"]:
                raise RuntimeError(f"{name}: expected no matches")
        else:
            matches = [m for m in report["matches"] if m["starts"] == [42]]
            if not matches or matches[0]["node_ids"] != sorted(expected):
                raise RuntimeError(f"{name}: unexpected captured source IDs")
            walk = matches[0]["walks"][0]
            if [node["id"] for node in walk["nodes"]] != expected:
                raise RuntimeError(f"{name}: unexpected traversal order")
            if name == "convolutions" and walk["quantifiers"][0]["count"] != 2:
                raise RuntimeError("Expected two repetitions of conv")
        print(f"{name}: {len(report['matches'])} match(es), verified; {output}")


if __name__ == "__main__":
    main()
