"""Import top-level ONNX operations using the official protobuf reader."""
from .graph import validate_graph
import math


def _attribute_value(attribute, onnx):
    """Expose declared scalar/list values; retain structured metadata for tensors."""
    kind = onnx.AttributeProto

    def string(value):
        try:
            return value.decode("utf-8")
        except UnicodeDecodeError:
            return {"bytes_hex": value.hex()}

    def number(value):
        return value if math.isfinite(value) else {"nonfinite": str(value)}

    def tensor(value):
        return {"kind": "tensor", "shape": list(value.dims), "data_type": value.data_type}

    converters = {
        kind.INT: lambda: attribute.i,
        kind.FLOAT: lambda: number(attribute.f),
        kind.STRING: lambda: string(attribute.s),
        kind.INTS: lambda: list(attribute.ints),
        kind.FLOATS: lambda: [number(value) for value in attribute.floats],
        kind.STRINGS: lambda: [string(value) for value in attribute.strings],
        kind.TENSOR: lambda: tensor(attribute.t),
        kind.TENSORS: lambda: [tensor(value) for value in attribute.tensors],
    }
    if attribute.ref_attr_name:
        return {"reference": attribute.ref_attr_name}
    return converters.get(attribute.type, lambda: {"onnx_attribute_type": attribute.type})()


def load_onnx(path):
    try:
        import onnx
    except ImportError as error:
        raise ValueError('ONNX support requires: python -m pip install "corgireg-cli[onnx]"') from error
    try:
        model = onnx.load_model(str(path), load_external_data=False)
    except Exception as error:
        raise ValueError(f"Cannot read ONNX model: {error}") from error
    graph = model.graph
    external = {value.name for value in graph.input}
    external.update(value.name for value in graph.initializer)
    external.update(value.values.name for value in graph.sparse_initializer)
    producers, nodes = {}, []
    for index, operation in enumerate(graph.node):
        if any(attr.type in (onnx.AttributeProto.GRAPH, onnx.AttributeProto.GRAPHS)
               for attr in operation.attribute):
            raise ValueError(f"Nested ONNX subgraphs are not supported (node {index}, {operation.op_type})")
        if not operation.op_type:
            raise ValueError(f"Missing operation type at node {index}")
        op = operation.op_type if operation.domain in ("", "ai.onnx") else f"{operation.domain}.{operation.op_type}"
        attributes = {}
        for attribute in operation.attribute:
            if attribute.name in attributes:
                raise ValueError(f"Duplicate ONNX attribute {attribute.name!r} at node {index}")
            attributes[attribute.name] = _attribute_value(attribute, onnx)
        nodes.append({"id": index, "op": op, "name": operation.name or f"node_{index}",
                      "inputs": list(operation.input), "outputs": list(operation.output),
                      "domain": operation.domain, "attributes": attributes})
        for output_index, output in enumerate(operation.output, 1):
            if not output:  # Optional absent outputs do not denote a value.
                continue
            if output in producers or output in external:
                raise ValueError(f"Multiple definitions of ONNX value {output!r}")
            producers[output] = (index, output_index)
    edges, connections = [], []
    for index, operation in enumerate(graph.node):
        for input_index, value in enumerate(operation.input, 1):
            if not value:
                continue
            if value in producers:
                source, output_index = producers[value]
                if source >= index:
                    raise ValueError(f"ONNX graph is not topologically ordered at node {index}")
                edges.append((source, index))
                connections.append({"from": source, "to": index, "input": input_index, "output": output_index})
            elif value not in external:
                raise ValueError(f"Undefined ONNX value {value!r} at node {index}")
    for output in graph.output:
        if output.name not in producers and output.name not in external:
            raise ValueError(f"Undefined ONNX graph output {output.name!r}")
    return validate_graph({"nodes": nodes, "edges": edges, "connections": connections})
