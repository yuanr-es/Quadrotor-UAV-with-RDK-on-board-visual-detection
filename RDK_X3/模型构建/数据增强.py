import cv2
import numpy as np
import os
import random
from glob import glob

RED_BALL_IMG = "E:/Byte_track_dataset/red_ball/images/train_ball"
RED_BALL_LABEL = "E:/Byte_track_dataset/red_ball/labels/train_ball"

OUT_IMG = "E:/Byte_track_dataset/red_ball/images/train"
OUT_LABEL = "E:/Byte_track_dataset/red_ball/labels/train"

AUG_PER_IMG = 4
MAX_IMG_W = 1280
MAX_IMG_H = 720

os.makedirs(OUT_IMG, exist_ok=True)
os.makedirs(OUT_LABEL, exist_ok=True)

def hsv_aug(img):
    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
    h, s, v = cv2.split(hsv)

    h = h.astype(np.int16)
    h += random.randint(-8, 8)
    h = np.clip(h, 0, 179).astype(np.uint8)

    s = s.astype(np.float32)
    s = s * random.uniform(0.55, 1.5)
    s = np.clip(s, 0, 255).astype(np.uint8)

    v = v.astype(np.float32)
    v = v * random.uniform(0.45, 1.7)
    v = np.clip(v, 0, 255).astype(np.uint8)

    hsv_merge = cv2.merge((h, s, v))
    return cv2.cvtColor(hsv_merge, cv2.COLOR_HSV2BGR)


def motion_blur_aug(img):
    if random.random() < 0.45:
        k = random.choice([3, 5])
        img = cv2.GaussianBlur(img, (k, k), 0)
    return img


def horizontal_flip(img, label_list):
    h, w = img.shape[:2]
    if random.random() < 0.5:
        img = cv2.flip(img, 1)
        new_labels = []
        for cls, x, y, bw, bh in label_list:
            x = 1.0 - x
            new_labels.append([cls, x, y, bw, bh])
        return img, new_labels
    return img, label_list


def random_scale_crop(img, label_list):
    h, w = img.shape[:2]
    scale = random.uniform(0.65, 0.98)
    nw, nh = int(w * scale), int(h * scale)
    img_resize = cv2.resize(img, (nw, nh))

    off_x = (w - nw) // 2
    off_y = (h - nh) // 2

    canvas = np.zeros((h, w, 3), dtype=np.uint8)
    canvas[off_y:off_y + nh, off_x:off_x + nw] = img_resize

    new_labels = []
    for cls, x, y, bw, bh in label_list:
        nx = x * scale + off_x / w
        ny = y * scale + off_y / h
        nbw = bw * scale
        nbh = bh * scale
        nx = np.clip(nx, 0.01, 0.99)
        ny = np.clip(ny, 0.01, 0.99)
        nbw = np.clip(nbw, 0.02, 0.98)
        nbh = np.clip(nbh, 0.02, 0.98)
        new_labels.append([cls, nx, ny, nbw, nbh])
    return canvas, new_labels

def random_occlusion(img, label_list):
    h, w = img.shape[:2]
    if random.random() < 0.3:
        ox1 = random.randint(0, int(w * 0.22))
        oy1 = random.randint(0, int(h * 0.22))
        ox2 = random.randint(int(w * 0.78), w)
        oy2 = random.randint(int(h * 0.78), h)
        crop_w = ox2 - ox1
        crop_h = oy2 - oy1
        crop_img = img[oy1:oy2, ox1:ox2]
        crop_img = cv2.resize(crop_img, (w, h))
        new_labels = []
        for cls, x, y, bw, bh in label_list:
            nx = (x * w - ox1) / crop_w
            ny = (y * h - oy1) / crop_h
            nbw = bw * w / crop_w
            nbh = bh * h / crop_h
            if 0 < nx < 1 and 0 < ny < 1:
                new_labels.append([cls, nx, ny, nbw, nbh])
        return crop_img, new_labels
    return img, label_list

def load_yolo_label(txt_path):
    labels = []
    if not os.path.exists(txt_path):
        return labels
    with open(txt_path, "r", encoding="utf-8") as f:
        for line in f.readlines():
            line = line.strip()
            if not line:
                continue
            data = list(map(float, line.split()))
            c = int(data[0])
            x, y, w, h = data[1], data[2], data[3], data[4]
            labels.append([c, x, y, w, h])
    return labels


def save_img_label(image, labels, img_save, txt_save):
    cv2.imwrite(img_save, image)
    with open(txt_save, "w", encoding="utf-8") as f:
        for lab in labels:
            c, x, y, bw, bh = lab
            f.write(f"{c} {x:.6f} {y:.6f} {bw:.6f} {bh:.6f}\n")

def augment_single_ball(img_path, lab_path, file_idx):
    src_img = cv2.imread(img_path)
    if src_img is None:
        print(f"跳过损坏图片: {img_path}")
        return

    h0, w0 = src_img.shape[:2]
    if w0 > MAX_IMG_W or h0 > MAX_IMG_H:
        src_img = cv2.resize(src_img, (MAX_IMG_W, MAX_IMG_H))

    base_name = os.path.basename(img_path).rsplit(".", 1)[0]
    raw_labels = load_yolo_label(lab_path)

    for aug_id in range(AUG_PER_IMG):
        aug_img = src_img.copy()
        aug_labs = raw_labels.copy()

        aug_img = hsv_aug(aug_img)
        aug_img = motion_blur_aug(aug_img)
        aug_img, aug_labs = horizontal_flip(aug_img, aug_labs)
        aug_img, aug_labs = random_scale_crop(aug_img, aug_labs)
        aug_img, aug_labs = random_occlusion(aug_img, aug_labs)

        save_name = f"{base_name}_aug_{file_idx}_{aug_id}"
        img_out = os.path.join(OUT_IMG, save_name + ".jpg")
        txt_out = os.path.join(OUT_LABEL, save_name + ".txt")
        save_img_label(aug_img, aug_labs, img_out, txt_out)

if __name__ == "__main__":
    img_list = glob(os.path.join(RED_BALL_IMG, "*.jpg")) + glob(os.path.join(RED_BALL_IMG, "*.png"))
    print(f"{len(img_list)}")
    for idx, img_p in enumerate(img_list):
        basename = os.path.basename(img_p).rsplit(".", 1)[0]
        label_p = os.path.join(RED_BALL_LABEL, basename + ".txt")
        augment_single_ball(img_p, label_p, idx)
    print("完成")