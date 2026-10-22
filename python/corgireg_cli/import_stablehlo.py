"""Read textual StableHLO through the MLIR bindings shipped with jaxlib."""
import os
import math
from pathlib import Path

from .graph import validate_graph

_dll_directory = None


def _attribute_value(attribute, ir):
    """Decode builtin MLIR values; keep dialect-specific attributes explicit."""
    if isinstance(attribute, (ir.StringAttr, ir.BoolAttr, ir.IntegerAttr)):
        return attribute.value
    if isinstance(attribute, ir.FloatAttr):
        value = attribute.value
        return value if math.isfinite(value) else {"nonfinite": str(value)}
    if isinstance(attribute, ir.ArrayAttr):
        return [_attribute_value(value, ir) for value in attribute]
    if isinstance(attribute, (ir.DenseBoolArrayAttr, ir.DenseI8ArrayAttr, ir.DenseI16ArrayAttr,
                              ir.DenseI32ArrayAttr, ir.DenseI64ArrayAttr,
                              ir.DenseF32ArrayAttr, ir.DenseF64ArrayAttr)):
        return [value if not isinstance(value, float) or math.isfinite(value)
                else {"nonfinite": str(value)} for value in attribute]
    if isinstance(attribute, (ir.DenseIntElementsAttr, ir.DenseFPElementsAttr)):
        shape = list(ir.ShapedType(attribute.type).shape)
        if math.prod(shape) > 4096:
            return {"kind": "dense_elements", "shape": shape,
                    "element_type": str(ir.ShapedType(attribute.type).element_type)}
        values = iter(attribute)

        def reshape(dimensions):
            if dimensions:
                return [reshape(dimensions[1:]) for _ in range(dimensions[0])]
            value = next(values)
            return value if not isinstance(value, float) or math.isfinite(value) else {"nonfinite": str(value)}

        return reshape(shape)
    return {"mlir": str(attribute)}


def load_stablehlo(path, function=None):
    global _dll_directory
    try:
        import jaxlib
        # Windows does not search a sibling package directory for jax_common.dll.
        if os.name == "nt" and _dll_directory is None:
            _dll_directory = os.add_dll_directory(str(Path(jaxlib.__file__).parent))
        from jaxlib.mlir import ir
        from jaxlib.mlir.dialects import stablehlo
        from jaxlib.mlir._mlir_libs import _jax_mlir_ext
    except ImportError as error:
        raise ValueError('StableHLO support requires working MLIR bindings: '
                         'python -m pip install "corgireg-cli[stablehlo]"') from error
    registry = ir.DialectRegistry()
    _jax_mlir_ext.register_dialects(registry)
    text = path.read_text(encoding="utf-8-sig")
    with ir.Context() as context:
        context.append_dialect_registry(registry)
        stablehlo.register_dialect(context)
        try:
            module = ir.Module.parse(text)
        except Exception as error:
            raise ValueError(f"Cannot parse textual StableHLO: {error}") from error
        functions = {}
        for operation in module.body.operations:
            if operation.operation.name != "func.func":
                raise ValueError(f"Unsupported top-level operation: {operation.operation.name}")
            name = ir.StringAttr(operation.attributes["sym_name"]).value
            functions[name] = operation
        if function is None:
            if "main" in functions:
                function = "main"
            elif len(functions) == 1:
                function = next(iter(functions))
            else:
                raise ValueError("Select a StableHLO function with --function")
        if function not in functions:
            raise ValueError(f"StableHLO function not found: {function}")
        selected = functions[function]
        if len(selected.regions) != 1 or len(selected.regions[0].blocks) != 1:
            raise ValueError("Only single-block StableHLO function bodies are supported")
        block = selected.regions[0].blocks[0]
        producers, nodes, edges = {}, [], []
        for index, operation in enumerate(block.operations):
            op = operation.operation.name
            if op == "func.return":
                continue
            if not op.startswith("stablehlo.") or len(operation.regions):
                raise ValueError(f"Unsupported operation or nested region: {op} (index {index})")
            results = [result.get_name() for result in operation.results]
            nodes.append({"id": index, "op": op, "name": f"{function}/" + (",".join(results) or f"op_{index}"),
                          "function": function, "results": results, "location": str(operation.location),
                          "inputs": [operand.get_name() for operand in operation.operands],
                          "attributes": {name: _attribute_value(operation.attributes[name], ir)
                                         for name in operation.attributes}})
            for operand in operation.operands:
                if operand in producers:
                    edges.append((producers[operand], index))
                elif not any(operand == argument for argument in block.arguments):
                    raise ValueError(f"Unknown StableHLO operand at operation {index}")
            for result in operation.results:
                producers[result] = index
        return validate_graph({"nodes": nodes, "edges": edges})
