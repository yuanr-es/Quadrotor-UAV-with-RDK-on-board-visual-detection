'''
from ultralytics import YOLO
import torch
torch.cuda.empty_cache()
model = YOLO('yolov5nu.pt')
if __name__ == '__main__':
    results = model.train(data = 'voc.yaml',epochs=24, batch=10, pretrained=False,device = 0)
'''
from ultralytics import YOLO
import torch

torch.cuda.empty_cache()

if __name__ == '__main__':
    model = YOLO('yolov5nu.pt')
    results = model.train(
        data='voc.yaml',
        imgsz=672,
        epochs=32,
        batch=10,
        pretrained=True,
        device=0,

        multi_scale=True,
        mosaic=1.0,
        mixup=0.0,
        hsv_h=0.015, hsv_s=0.7, hsv_v=0.4,

        amp=True,
        cache=False,
        workers=4,

        rect=False,
        cls=0.5,
        box=7.5,
        dfl=1.5,

        save=True,
        patience=6,

        conf=0.4,
        iou=0.6,
    )