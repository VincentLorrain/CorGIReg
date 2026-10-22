import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from .graph import encode_request, load_json


def positive_int(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return number


def parser():
    cli = argparse.ArgumentParser(description="Match a CorGIReg pattern against an external operation graph.")
    cli.add_argument("-i", "--input", type=Path, required=True, help="source graph file")
    cli.add_argument("-f", "--format", choices=["json"], help="input format (otherwise inferred from extension)")
    regex = cli.add_mutually_exclusive_group(required=True)
    regex.add_argument("-r", "--regex", help="complete graph pattern")
    regex.add_argument("-R", "--regex-file", type=Path, help="UTF-8 file containing one complete graph pattern")
    cli.add_argument("-o", "--output", default="-", help="JSON output file; '-' writes to stdout (default)")
    cli.add_argument("--engine", type=Path, help="path to the corgireg-match native executable")
    cli.add_argument("--max-starts", type=positive_int, default=100000, help="maximum starting tuples to search")
    cli.add_argument("--timeout", type=positive_int, default=60, help="matching timeout in seconds")
    return cli


def find_engine(explicit=None):
    if explicit:
        candidate = explicit
    elif os.environ.get("CORGIREG_ENGINE"):
        candidate = Path(os.environ["CORGIREG_ENGINE"])
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


def run(args):
    query = args.regex if args.regex is not None else args.regex_file.read_text(encoding="utf-8-sig")
    if not query.strip():
        raise ValueError("The regex must not be empty")
    if args.output != "-":
        destination = Path(args.output)
        for source in (args.input, args.regex_file):
            if source is not None and (destination.resolve() == source.resolve() or
                                      (destination.exists() and source.exists() and destination.samefile(source))):
                raise ValueError("Output must not overwrite an input file")
    format_name = args.format or {".json": "json"}.get(args.input.suffix.lower())
    if format_name != "json":
        raise ValueError("Unknown input format; specify --format")
    graph = load_json(args.input)
    process = subprocess.run([str(find_engine(args.engine))], input=encode_request(graph, query, args.max_starts),
                             text=True, encoding="utf-8", capture_output=True, timeout=args.timeout, check=False)
    if process.returncode:
        raise ValueError(process.stderr.strip() or f"Native matching failed ({process.returncode})")
    result = json.loads(process.stdout)
    result.update(schema_version=1, query=query, source=str(args.input.resolve()), format=format_name,
                  graph=graph)
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
