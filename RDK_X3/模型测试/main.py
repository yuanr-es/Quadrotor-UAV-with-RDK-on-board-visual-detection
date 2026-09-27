import cv2
import numpy as np
from ultralytics import YOLO
from yolox.tracker.byte_tracker import BYTETracker

class Args:
    track_thresh = 0.4
    track_buffer = 30
    match_thresh = 0.8
    min_box_area = 10
    mot20 = False


track_args = Args()
tracker = BYTETracker(track_args, frame_rate=30)

model = YOLO("demo05.pt")
cap = cv2.VideoCapture("test_vidio/car_test.mp4")

fps = cap.get(cv2.CAP_PROP_FPS)
width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
fourcc = cv2.VideoWriter_fourcc(*'mp4v')
out = cv2.VideoWriter("track_result.mp4", fourcc, fps, (width, height))

while cap.isOpened():
    ret, frame = cap.read()
    if not ret:
        break
    frame_show = frame.copy()
    img_h, img_w = frame.shape[:2]

    yolo_res = model(frame, conf=0.25)[0]
    yolo_boxes = yolo_res.boxes.data.cpu().numpy()
    dets_yolo = yolo_boxes[:, :5]

    img_info = [img_h, img_w, 1.0]
    img_size = [img_h, img_w]
    online_targets = tracker.update(dets_yolo, img_info, img_size)

    for target in online_targets:
        tlwh = target.tlwh  # [x,y,w,h]
        tid = target.track_id
        x1, y1 = int(tlwh[0]), int(tlwh[1])
        x2, y2 = int(tlwh[0] + tlwh[2]), int(tlwh[1] + tlwh[3])
        cv2.rectangle(frame_show, (x1, y1), (x2, y2), (0, 255, 0), 2)
        cv2.putText(frame_show, f"ID:{tid}", (x1, y1 - 6),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)

    out.write(frame_show)

    cv2.imshow("ByteTrack Demo | Q=退出", frame_show)
    key = cv2.waitKey(1) & 0xFF
    if key == ord("q"):
        break

cap.release()
out.release()
cv2.destroyAllWindows()
