from ultralytics import YOLO
import onnx
from onnxsim import simplify

WEIGHT_PATH = "D:/PythonProject/Byte_track_demo/demo05.pt"
EXPORT_IMGSZ = 672
OPSET_VERSION = 11
SIMPLIFY_ONNX = True

if __name__ == "__main__":

    model = YOLO(WEIGHT_PATH)

    export_result = model.export(
        format="onnx",
        imgsz=EXPORT_IMGSZ,
        opset=OPSET_VERSION,
        simplify=False,
        device="cpu",
        dynamic=False,
        nms=False,
    )
    raw_onnx_path = export_result
    print(f"原始未简化ONNX导出完成：{raw_onnx_path}")

    if SIMPLIFY_ONNX:
        onnx_model = onnx.load(raw_onnx_path)
        simplified_model, check_ok = simplify(onnx_model)
        if not check_ok:
            raise RuntimeError("ONNX模型简化失败")
        simplified_onnx_path = raw_onnx_path.replace(".onnx", "_simplified.onnx")
        onnx.save(simplified_model, simplified_onnx_path)
        print(f"简化后ONNX文件生成完成：{simplified_onnx_path}")
        onnx.checker.check_model(simplified_model)
        print("模型校验通过")
    else:
        onnx.checker.check_model(raw_onnx_path)
        print("原始ONNX模型校验通过")