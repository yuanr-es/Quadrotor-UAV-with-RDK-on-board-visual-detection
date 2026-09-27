from ultralytics import YOLO
import os

model_path = "ball_model_2.pt"
img_dir = "E:/Red_ball_vidio/images/val"
label_out = "E:/Red_ball_vidio/labels/val"
conf_thresh = 0.3

os.makedirs(label_out, exist_ok=True)
model = YOLO(model_path)

results = model.predict(
    source=img_dir,
    conf=conf_thresh,
    save=False,
    verbose=False
)

for res in results:
    img_name = os.path.splitext(os.path.basename(res.path))[0]
    txt_path = os.path.join(label_out, f"{img_name}.txt")
    lines = []
    boxes = res.boxes.cpu()
    if boxes is None:
        continue
    cls_ids = boxes.cls.numpy()
    xywhn = boxes.xywhn.numpy()
    confs = boxes.conf.numpy()

    for cls, box, conf in zip(cls_ids, xywhn, confs):
        if int(cls) != 0:
            continue
        if conf < conf_thresh:
            continue
        cx, cy, w, h = box
        line = f"{int(cls)} {cx:.6f} {cy:.6f} {w:.6f} {h:.6f}\n"
        lines.append(line)
    with open(txt_path, "w", encoding="utf-8") as f:
        f.writelines(lines)

print(f"自动标注完成，标签保存在 {label_out}")