import cv2
from ultralytics import YOLO

VIDEO_PATH = "D:/PythonProject/Byte_track_demo/demo_1.mp4"
MODEL_PATH = "D:/PythonProject/Byte_track_demo/best.pt"
CONF_THRESH = 0.45
CLASS_NAMES = ["person", "red_ball", "car"]

def video_detect_only_show():
    model = YOLO(MODEL_PATH)
    cap = cv2.VideoCapture(VIDEO_PATH)
    if not cap.isOpened():
        print("视频打开失败，请检查路径")
        return

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break
        results = model(frame, conf=CONF_THRESH, save=False, show=False, verbose=False)
        res = results[0]
        boxes = res.boxes

        for box in boxes:
            cid = int(box.cls)
            conf = float(box.conf)
            x1, y1, x2, y2 = map(int, box.xyxy[0])
            label = f"{CLASS_NAMES[cid]} {conf:.2f}"

            cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)

            cv2.rectangle(frame, (x1, y1 - 20), (x1 + 180, y1), (0, 255, 0), -1)
            cv2.putText(frame, label, (x1 + 5, y1 - 5), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)

        cv2.imshow("YOLO Detection Window", frame)

        if cv2.waitKey(1) & 0xFF == 27:
            break

    cap.release()
    cv2.destroyAllWindows()
    print("检测结束")

if __name__ == "__main__":
    video_detect_only_show()