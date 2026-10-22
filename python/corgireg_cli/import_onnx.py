"""Import top-level ONNX operations using the official protobuf reader."""
from .graph import validate_graph


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
        nodes.append({"id": index, "op": op, "name": operation.name or f"node_{index}",
                      "inputs": list(operation.input), "outputs": list(operation.output),
                      "domain": operation.domain})
        for output in operation.output:
            if not output:  # Optional absent outputs do not denote a value.
                continue
            if output in producers or output in external:
                raise ValueError(f"Multiple definitions of ONNX value {output!r}")
            producers[output] = index
    edges = []
    for index, operation in enumerate(graph.node):
        for value in operation.input:
            if not value:
                continue
            if value in producers:
                if producers[value] >= index:
                    raise ValueError(f"ONNX graph is not topologically ordered at node {index}")
                edges.append((producers[value], index))
            elif value not in external:
                raise ValueError(f"Undefined ONNX value {value!r} at node {index}")
    for output in graph.output:
        if output.name not in producers and output.name not in external:
            raise ValueError(f"Undefined ONNX graph output {output.name!r}")
    return validate_graph({"nodes": nodes, "edges": edges})
