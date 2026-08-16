import onnx
from onnx import helper, TensorProto


input_tensor = helper.make_tensor_value_info(
    "input",
    TensorProto.FLOAT,
    ["batch", 3]
)

output_tensor = helper.make_tensor_value_info(
    "output",
    TensorProto.FLOAT,
    ["batch", 3]
)

scale = helper.make_tensor(
    name="scale",
    data_type=TensorProto.FLOAT,
    dims=[1],
    vals=[2.0]
)

mul_node = helper.make_node(
    "Mul",
    inputs=["input", "scale"],
    outputs=["output"]
)

graph = helper.make_graph(
    nodes=[mul_node],
    name="Mul2Graph",
    inputs=[input_tensor],
    outputs=[output_tensor],
    initializer=[scale]
)

model = helper.make_model(
    graph,
    producer_name="MiniServe"
)

# 使用较新的 IR/opset 设置
model.opset_import[0].version = 18
model.ir_version = 8

onnx.checker.check_model(model)

onnx.save(
    model,
    "models/mul2.onnx"
)

print("Created models/mul2.onnx")
