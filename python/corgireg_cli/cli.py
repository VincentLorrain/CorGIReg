import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from .graph import encode_request, load_json
from .node_library import library_arguments


def positive_int(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return number


def parser():
    cli = argparse.ArgumentParser(description="Match a CorGIReg pattern against an external operation graph.")
    cli.add_argument("-i", "--input", type=Path, required=True, help="source graph file")
    cli.add_argument("-f", "--format", choices=["json", "onnx", "stablehlo"], help="input format (otherwise inferred from extension)")
    cli.add_argument("--function", help="StableHLO function (default: main, or the only function)")
    cli.add_argument("--node-library", action="append", type=Path, default=[],
                     help="JSON node definition file or directory (repeatable)")
    regex = cli.add_mutually_exclusive_group(required=True)
    regex.add_argument("-r", "--regex", help="complete graph pattern")
    regex.add_argument("-R", "--regex-file", type=Path, help="UTF-8 file containing one complete graph pattern")
    cli.add_argument("-o", "--output", default="-", help="JSON output file; '-' writes to stdout (default)")
    cli.add_argument("--engine", type=Path, help="path to the corgireg-match native executable")
    cli.add_argument("--max-starts", type=positive_int, default=100000, help="maximum starting tuples to search")
    cli.add_argument("--timeout", type=positive_int, default=60, help="matching timeout in seconds")
    cli.add_argument("-j", "--jobs", type=worker_count, default=1,
                     help="parallel starting-tuple workers, 1..256 (default: 1)")
    return cli


def worker_count(value):
    number = positive_int(value)
    if number > 256:
        raise argparse.ArgumentTypeError("must be between 1 and 256")
    return number


def find_engine(explicit=None):
    if explicit:
        candidate = explicit
    elif os.environ.get("CORGIREG_ENGINE"):
        candidate = Path(os.environ["CORGIREG_ENGINE"])
    elif (Path(sys.prefix) / "bin" / ("corgireg-match.exe" if os.name == "nt" else "corgireg-match")).is_file():
        candidate = Path(sys.prefix) / "bin" / ("corgireg-match.exe" if os.name == "nt" else "corgireg-match")
    elif shutil.which("corgireg-match"):
        candidate = Path(shutil.which("corgireg-match"))
    else:
        suffix = ".exe" if os.name == "nt" else ""
        candidate = Path(__file__).resolve().parents[2] / "build" / ("corgireg-match" + suffix)
    if not candidate.is_file():
        raise ValueError("Native engine not found: build the project, then use --engine or CORGIREG_ENGINE")
    return candidate.resolve()


def write_report(path, report):
    # Do not truncate an existing report if matching or serialization fails.
    text = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if path == "-":
        sys.stdout.write(text)
        return
    output = Path(path)
    temp = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=output.resolve().parent,
                                         prefix=".corgireg-", delete=False) as stream:
            temp = Path(stream.name)
            stream.write(text)
        os.replace(temp, output)
    finally:
        if temp is not None and temp.exists():
            temp.unlink()


def check_output(path, sources):
    if path == "-":
        return
    destination = Path(path)
    for source in sources:
        if source is not None and (destination.resolve() == source.resolve() or
                                  (destination.exists() and source.exists() and destination.samefile(source))):
            raise ValueError("Output must not overwrite an input file")


def run(args):
    query = args.regex if args.regex is not None else args.regex_file.read_text(encoding="utf-8-sig")
    if not query.strip():
        raise ValueError("The regex must not be empty")
    check_output(args.output, [args.input, args.regex_file])
    format_name = args.format or {".json": "json", ".onnx": "onnx", ".mlir": "stablehlo",
                                 ".stablehlo": "stablehlo"}.get(args.input.suffix.lower())
    if args.function and format_name != "stablehlo":
        raise ValueError("--function is only available for StableHLO input")
    if format_name == "json":
        graph = load_json(args.input)
    elif format_name == "onnx":
        from .import_onnx import load_onnx
        graph = load_onnx(args.input)
    elif format_name == "stablehlo":
        from .import_stablehlo import load_stablehlo
        graph = load_stablehlo(args.input, args.function)
    else:
        raise ValueError("Unknown input format; specify --format")
    # The native engine loads, validates and resolves node definitions from the
    # node documents of protocol version 3 (empty definition section).
    # Inline conditions need source attributes even without a node library.
    request = encode_request(graph, query, args.max_starts, {}, {})
    command = [str(find_engine(args.engine))]
    if args.jobs != 1:
        command.extend(["--jobs", str(args.jobs)])
    if args.node_library:
        command.extend(library_arguments(args.node_library, format_name))
    process = subprocess.run(command, input=request,
                             text=True, encoding="utf-8", capture_output=True, timeout=args.timeout, check=False)
    if process.returncode:
        raise ValueError(process.stderr.strip() or f"Native matching failed ({process.returncode})")
    result = json.loads(process.stdout)
    if not args.node_library:
        result.pop("definition_nodes", None)
    result.update(schema_version=1, query=query, source=str(args.input.resolve()), format=format_name,
                  graph=graph)
    if args.node_library:
        result["node_definitions"] = result.pop("node_definitions")  # Keep the report's field order.
        check_output(args.output, [Path(value["source"]) for value in result["node_definitions"].values()])
    write_report(args.output, result)
    return 0


def main(argv=None):
    args = parser().parse_args(argv)
    try:
        return run(args)
    except subprocess.TimeoutExpired:
        print("corgireg: matching timed out; increase --timeout or narrow the pattern", file=sys.stderr)
        return 2
    except (OSError, ValueError, RuntimeError) as error:
        print(f"corgireg: {error}", file=sys.stderr)
        return 2
