"""Record native timings by Git revision and compare compatible benchmark reports."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def capture(command):
    return subprocess.run([str(item) for item in command], cwd=ROOT, check=True,
                          capture_output=True, text=True, encoding="utf-8").stdout


def source_state():
    revision = capture(["git", "rev-parse", "HEAD"]).strip()
    status = capture(["git", "status", "--porcelain", "-z"])
    digest = hashlib.sha256(capture(["git", "diff", "--binary", "HEAD"]).encode("utf-8"))
    for name in capture(["git", "ls-files", "--others", "--exclude-standard", "-z"]).split("\0"):
        if name and (ROOT / name).is_file():
            digest.update(name.encode("utf-8"))
            digest.update((ROOT / name).read_bytes())
    return {"revision": revision, "dirty": bool(status), "worktree_sha256": digest.hexdigest(),
            "changes": [entry for entry in status.split("\0") if entry]}


def summary(samples):
    if not samples or any(type(value) not in (int, float) or not math.isfinite(value) or value <= 0 for value in samples):
        raise ValueError("Timing samples must be finite positive numbers")
    ordered = sorted(samples)
    return {"median_ns": statistics.median(ordered), "min_ns": ordered[0], "max_ns": ordered[-1],
            "p95_ns": ordered[math.ceil(0.95 * len(ordered)) - 1],
            "mad_ns": statistics.median(abs(value - statistics.median(ordered)) for value in ordered)}


def compare_reports(baseline, current, threshold=20):
    if not math.isfinite(threshold) or threshold < 0:
        raise ValueError("Threshold must be a finite nonnegative percentage")
    if baseline.get("schema_version") != 1 or current.get("schema_version") != 1:
        raise ValueError("Unsupported report schema")
    # A different source commit is expected; a different workload or environment is not.
    for field in ("suite_sha256", "machine", "build", "sampling"):
        if field not in baseline or field not in current or baseline[field] != current[field]:
            raise ValueError(f"Incompatible reports: {field} differs")

    def indexed(report):
        cases = report.get("cases", [])
        if not cases or any(not isinstance(case.get("name"), str) for case in cases):
            raise ValueError("Report must contain named benchmark cases")
        result = {case["name"]: case for case in cases}
        if len(result) != len(cases):
            raise ValueError("Duplicate benchmark case name")
        return result

    old, new = indexed(baseline), indexed(current)
    if old.keys() != new.keys():
        raise ValueError("Incompatible benchmark case sets")
    changes = []
    for name in old:
        for field in ("graph_nodes", "start_arity", "expected_result"):
            if old[name].get(field) != new[name].get(field):
                raise ValueError(f"Incompatible workload for {name}: {field}")
        before, after = summary(old[name]["samples_ns"]), summary(new[name]["samples_ns"])
        if len(old[name]["samples_ns"]) != baseline["sampling"]["samples"] or len(new[name]["samples_ns"]) != current["sampling"]["samples"]:
            raise ValueError(f"Incorrect sample count for {name}")
        percent = (after["median_ns"] / before["median_ns"] - 1) * 100
        status = "slower" if percent > threshold else "faster" if percent < -threshold else "within_threshold"
        changes.append({"name": name, "baseline_median_ns": before["median_ns"],
                        "current_median_ns": after["median_ns"], "change_percent": percent,
                        "baseline_mad_ns": before["mad_ns"], "current_mad_ns": after["mad_ns"], "status": status})
    return {"threshold_percent": threshold, "baseline_source": baseline.get("source"),
            "current_source": current.get("source"), "changes": changes,
            "regressions": [case["name"] for case in changes if case["status"] == "slower"]}


def write_new(path, report):
    path.parent.mkdir(parents=True, exist_ok=True)
    # Reports/baselines are immutable: do not silently overwrite a reference run.
    with path.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")


def read_report(path):
    return json.loads(path.read_text(encoding="utf-8"))


def measure(args):
    if args.output and args.output.exists():
        raise ValueError(f"Report already exists: {args.output}")
    before = source_state()
    build = args.build_dir.resolve()
    configure = ["cmake", "-S", ROOT, "-B", build, f"-DCMAKE_BUILD_TYPE={args.config}", "-DCORGIREG_BUILD_BENCHMARKS=ON"]
    generator = args.generator
    if not generator and os.name == "nt" and not (build / "CMakeCache.txt").exists():
        if shutil.which("g++") and shutil.which("mingw32-make"):
            generator = "MinGW Makefiles"
    if generator:
        configure.extend(["-G", generator])
    for command in (configure, ["cmake", "--build", build, "--config", args.config,
                               "--target", "corgireg-benchmark", "--parallel", args.jobs]):
        subprocess.run([str(item) for item in command], cwd=ROOT, check=True)
    filename = "corgireg-benchmark.exe" if os.name == "nt" else "corgireg-benchmark"
    executable = build / args.config / filename
    if not executable.exists():
        executable = build / filename
    raw = json.loads(capture([executable, "--samples", args.samples, "--iterations", args.iterations]))
    after = source_state()
    if before != after:
        raise ValueError("Sources changed during compilation/measurement; discard this run and retry")
    if raw["configuration"] != args.config:
        raise ValueError("Benchmark binary configuration differs from requested configuration")
    cache = {}
    for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if "=" in line and ":" in line.split("=", 1)[0] and not line.startswith(("#", "//")):
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    flag_names = ["CMAKE_CXX_FLAGS", "CMAKE_CXX_FLAGS_" + args.config.upper(),
                  "CMAKE_EXE_LINKER_FLAGS", "CMAKE_EXE_LINKER_FLAGS_" + args.config.upper(),
                  "CMAKE_INTERPROCEDURAL_OPTIMIZATION", "CMAKE_GENERATOR"]
    timestamp = datetime.now(timezone.utc)
    report = {"schema_version": 1, "recorded_at": timestamp.isoformat(), "source": before,
              "suite_sha256": hashlib.sha256((ROOT / "benchmarks/native.cpp").read_bytes()).hexdigest(),
              "machine": {"name": args.machine or platform.node(), "system": platform.system(),
                          "release": platform.release(), "architecture": platform.machine(),
                          "processor": platform.processor(), "logical_cpus": os.cpu_count()},
              "build": {"configuration": raw["configuration"], "compiler": raw["compiler"],
                        "flags": {name: cache.get(name, "") for name in flag_names}},
              "sampling": {"samples": args.samples, "iterations": raw["iterations"], "warmups": raw["warmups"]},
              "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
              "binary_bytes": executable.stat().st_size, "cases": raw["cases"]}
    for case in report["cases"]:
        case.update(summary(case["samples_ns"]))
    if args.baseline:
        report["comparison"] = compare_reports(read_report(args.baseline), report, args.threshold)
    output = args.output or build / "reports" / (timestamp.strftime("%Y%m%dT%H%M%S%fZ") + "-" + before["revision"][:12] + ".json")
    write_new(output, report)
    print(f"Report: {output}")
    if before["dirty"]:
        print("NOTE: working tree is modified; this report describes the recorded source fingerprint, not the clean commit.")
    for case in report["cases"]:
        print(f"  {case['name']}: median {case['median_ns'] / 1e6:.4f} ms; MAD {case['mad_ns'] / 1e6:.4f} ms")
    return report.get("comparison")


def positive(value):
    number = int(value)
    if not 1 <= number <= 1000:
        raise argparse.ArgumentTypeError("value must be between 1 and 1000")
    return number


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    run = commands.add_parser("run", help="Rebuild, measure and save a new report")
    run.add_argument("--build-dir", type=Path, default=ROOT / "build/performance")
    run.add_argument("--config", choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"], default="Release")
    run.add_argument("--generator")
    run.add_argument("--jobs", type=positive, default=2)
    run.add_argument("--samples", type=positive, default=7)
    run.add_argument("--iterations", type=positive, default=3)
    run.add_argument("--machine", help="Stable identifier for the same physical benchmark machine")
    run.add_argument("--baseline", type=Path)
    compare = commands.add_parser("compare", help="Compare two saved reports without rebuilding")
    compare.add_argument("baseline", type=Path)
    compare.add_argument("current", type=Path)
    for command in (run, compare):
        command.add_argument("--output", type=Path)
        command.add_argument("--threshold", type=float, default=20.0)
        command.add_argument("--fail-on-regression", action="store_true")
    args = parser.parse_args(argv)
    try:
        if not math.isfinite(args.threshold) or args.threshold < 0:
            raise ValueError("Threshold must be a finite nonnegative percentage")
        if args.command == "run":
            if args.fail_on_regression and not args.baseline:
                raise ValueError("--fail-on-regression requires --baseline when measuring")
            comparison = measure(args)
        else:
            comparison = compare_reports(read_report(args.baseline), read_report(args.current), args.threshold)
            if args.output:
                write_new(args.output, comparison)
        if comparison:
            for item in comparison["changes"]:
                print(f"{item['name']}: {item['change_percent']:+.1f}% ({item['status']})")
            if args.fail_on_regression and comparison["regressions"]:
                return 2
        return 0
    except (OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        print(f"Benchmark error: {error}", file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError) and error.stderr:
            print(error.stderr, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
