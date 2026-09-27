from pathlib import Path
import cv2
import numpy as np

SOURCE_IMG_DIR = r"E:\calib_jpg"
OUTPUT_NPY_DIR = r"E:\calib_npy"
INPUT_SIZE = 512

Path(OUTPUT_NPY_DIR).mkdir(exist_ok=True)

def letterbox(im, new_shape=(512, 512), color=(114, 114, 114), auto=False, scaleup=True):

    shape = im.shape[:2]
    r = min(new_shape[0] / shape[0], new_shape[1] / shape[1])
    if not scaleup:
        r = min(r, 1.0)

    new_unpad = int(round(shape[1] * r)), int(round(shape[0] * r))
    dw, dh = new_shape[1] - new_unpad[0], new_shape[0] - new_unpad[1]
    dw /= 2
    dh /= 2

    im = cv2.resize(im, new_unpad, interpolation=cv2.INTER_LINEAR)
    top, bottom = int(round(dh - 0.1)), int(round(dh + 0.1))
    left, right = int(round(dw - 0.1)), int(round(dw + 0.1))
    im = cv2.copyMakeBorder(im, top, bottom, left, right, cv2.BORDER_CONSTANT, value=color)
    return im

img_suffix = ("*.jpg", "*.png", "*.jpeg")
img_paths = []
for suffix in img_suffix:
    img_paths.extend(Path(SOURCE_IMG_DIR).glob(suffix))

if len(img_paths) == 0:
    print("未找到图片，请检查路径！")
    exit()

print(f"一共找到 {len(img_paths)} 张校准图片，开始生成npy……")

for idx, img_path in enumerate(img_paths):
    img = cv2.imread(str(img_path))
    img = letterbox(img, (INPUT_SIZE, INPUT_SIZE))
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    img = img.astype(np.float32) / 255.0
    img = np.transpose(img, (2, 0, 1))
    img = np.expand_dims(img, axis=0)
    save_name = Path(img_path).stem + ".npy"
    save_path = Path(OUTPUT_NPY_DIR) / save_name
    np.save(str(save_path), img)

    if (idx + 1) % 10 == 0:
        print(f"已处理 {idx+1}/{len(img_paths)}")
print("全部处理完成！")