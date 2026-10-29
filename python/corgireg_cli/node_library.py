"""Node-definition libraries are loaded, validated and resolved by the native engine."""
import json
import subprocess


def library_arguments(paths, format_name=None):
    """corgireg-match options selecting definition files/directories and the input format."""
    arguments = []
    for path in paths:
        arguments.extend(["--node-library", str(path)])
    if format_name is not None:
        arguments.extend(["--format", format_name])
    return arguments


def load_definitions(paths, engine=None):
    """Validate definitions natively; return name -> schema_version, name, description, formats, source."""
    from .cli import find_engine
    process = subprocess.run([str(find_engine(engine)), *library_arguments(paths), "--list-definitions"],
                             text=True, encoding="utf-8", capture_output=True, check=False)
    if process.returncode:
        raise ValueError(process.stderr.strip() or f"Native node library loading failed ({process.returncode})")
    return json.loads(process.stdout)
