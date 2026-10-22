"""Run and verify a matching example using native NodeTest attribute rules."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True, help="JSON report path (parent must exist)")
    parser.add_argument("--engine", type=Path)
    args = parser.parse_args()
    command = [sys.executable, "-m", "corgireg_cli",
               "-i", str(HERE / "graphs/conditional.json"),
               "--node-library", str(HERE.parent / "library/nodes"),
               "--node-library", str(HERE.parent / "library/conditional-nodes"),
               "-R", str(HERE / "patterns/conditional.txt"), "-o", str(args.output)]
    if args.engine:
        command.extend(["--engine", str(args.engine)])
    subprocess.run(command, check=True, timeout=90)
    report = json.loads(args.output.read_text(encoding="utf-8"))
    if [match["node_ids"] for match in report["matches"]] != [[0, 42]]:
        raise RuntimeError("Expected only the 3x3 convolution without debug attributes")
    if report["node_definitions"]["conv3x3"]["node_ids"] != [42]:
        raise RuntimeError("Unexpected filtered predicate membership")
    print(f"Verified match [42 -> 0]; rejected 1x1 and debug-tagged convolutions. Report: {args.output}")


if __name__ == "__main__":
    main()
