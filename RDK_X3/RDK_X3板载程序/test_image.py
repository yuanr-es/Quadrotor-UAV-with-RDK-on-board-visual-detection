#!/usr/bin/env python3
import numpy as np
import cv2
from hobot_dnn import pyeasy_dnn as dnn
import time
import ctypes
import json
import os

class hbSysMem_t(ctypes.Structure):
    _fields_ = [
        ("phyAddr", ctypes.c_double),
        ("virAddr", ctypes.c_void_p),
        ("memSize", ctypes.c_int)
    ]


class hbDNNQuantiShift_yt(ctypes.Structure):
    _fields_ = [
        ("shiftLen", ctypes.c_int),
        ("shiftData", ctypes.c_char_p)
    ]


class hbDNNQuantiScale_t(ctypes.Structure):
    _fields_ = [
        ("scaleLen", ctypes.c_int),
        ("scaleData", ctypes.POINTER(ctypes.c_float)),
        ("zeroPointLen", ctypes.c_int),
        ("zeroPointData", ctypes.c_char_p)
    ]


class hbDNNTensorShape_t(ctypes.Structure):
    _fields_ = [
        ("dimensionSize", ctypes.c_int * 8),
        ("numDimensions", ctypes.c_int)
    ]


class hbDNNTensorProperties_t(ctypes.Structure):
    _fields_ = [
        ("validShape", hbDNNTensorShape_t),
        ("alignedShape", hbDNNTensorShape_t),
        ("tensorLayout", ctypes.c_int),
        ("tensorType", ctypes.c_int),
        ("shift", hbDNNQuantiShift_yt),
        ("scale", hbDNNQuantiScale_t),
        ("quantiType", ctypes.c_int),
        ("quantizeAxis", ctypes.c_int),
        ("alignedByteSize", ctypes.c_int),
        ("stride", ctypes.c_int * 8)
    ]


class hbDNNTensor_t(ctypes.Structure):
    _fields_ = [
        ("sysMem", hbSysMem_t * 4),
        ("properties", hbDNNTensorProperties_t)
    ]


class Yolov5PostProcessInfo_t(ctypes.Structure):
    _fields_ = [
        ("height", ctypes.c_int),
        ("width", ctypes.c_int),
        ("ori_height", ctypes.c_int),
        ("ori_width", ctypes.c_int),
        ("score_threshold", ctypes.c_float),
        ("nms_threshold", ctypes.c_float),
        ("nms_top_k", ctypes.c_int),
        ("is_pad_resize", ctypes.c_int)
    ]

LIB_PATH = './postprocess/build/libpostprocess.so'
if not os.path.exists(LIB_PATH):
    raise FileNotFoundError(f"后处理库不存在：{LIB_PATH}")
libpostprocess = ctypes.CDLL(LIB_PATH)

libpostprocess.Yolov5doProcess.argtypes = [
    ctypes.POINTER(hbDNNTensor_t),
    ctypes.POINTER(Yolov5PostProcessInfo_t),
    ctypes.c_int
]
libpostprocess.Yolov5doProcess.restype = None

libpostprocess.Yolov5PostProcess.argtypes = [ctypes.POINTER(Yolov5PostProcessInfo_t)]
libpostprocess.Yolov5PostProcess.restype = ctypes.c_char_p

get_Postprocess_result = libpostprocess.Yolov5PostProcess
do_process_one_output = libpostprocess.Yolov5doProcess

def get_TensorLayout(Layout):
    if Layout == "NCHW":
        return int(2)
    else:
        return int(0)

def bgr2nv12_opencv(image):
    height, width = image.shape[0], image.shape[1]
    area = height * width
    yuv420p = cv2.cvtColor(image, cv2.COLOR_BGR2YUV_I420).reshape((area * 3 // 2,))
    y = yuv420p[:area]
    uv_planar = yuv420p[area:].reshape((2, area // 4))
    uv_packed = uv_planar.transpose((1, 0)).reshape((area // 2,))
    nv12 = np.zeros_like(yuv420p)
    nv12[:height * width] = y
    nv12[height * width:] = uv_packed
    return nv12

def get_hw(pro):
    if pro.layout == "NCHW":
        return pro.shape[2], pro.shape[3]
    else:
        return pro.shape[1], pro.shape[2]

def letterbox(im, new_shape=(672, 672), color=(114, 114, 114)):
    shape = im.shape[:2]  # (h, w)
    r = min(new_shape[0] / shape[0], new_shape[1] / shape[1])
    new_unpad = int(round(shape[1] * r)), int(round(shape[0] * r))
    dw, dh = new_shape[1] - new_unpad[0], new_shape[0] - new_unpad[1]
    dw /= 2
    dh /= 2
    if shape[::-1] != new_unpad:
        im = cv2.resize(im, new_unpad, interpolation=cv2.INTER_LINEAR)
    top, bottom = int(round(dh - 0.1)), int(round(dh + 0.1))
    left, right = int(round(dw - 0.1)), int(round(dw + 0.1))
    im = cv2.copyMakeBorder(im, top, bottom, left, right, cv2.BORDER_CONSTANT, value=color)
    return im, r, (dw, dh)


if __name__ == '__main__':
    # 加载模型
    MODEL_PATH = '../models/yolov5s_672x672_nv12_NHWC_simplified.bin'
    models = dnn.load(MODEL_PATH)

    # 读取图片
    img_file = cv2.imread('./car_data_1_8613.jpg')
    if img_file is None:
        raise FileNotFoundError("测试图片不存在")

    h, w = get_hw(models[0].inputs[0].properties)

    resized_data, ratio, (dw, dh) = letterbox(img_file, (h, w))
    nv12_data = bgr2nv12_opencv(resized_data)

    outputs = models[0].forward(nv12_data)

    yolov5_postprocess_info = Yolov5PostProcessInfo_t()
    yolov5_postprocess_info.height = h
    yolov5_postprocess_info.width = w
    org_height, org_width = img_file.shape[0:2]
    yolov5_postprocess_info.ori_height = org_height
    yolov5_postprocess_info.ori_width = org_width
    yolov5_postprocess_info.score_threshold = 0.25
    yolov5_postprocess_info.nms_threshold = 0.45
    yolov5_postprocess_info.nms_top_k = 200
    yolov5_postprocess_info.is_pad_resize = 1  # 【关键】设为1，启用letterbox坐标还原

    output_tensors = (hbDNNTensor_t * len(models[0].outputs))()

    for i in range(len(models[0].outputs)):
        output_tensors[i].properties.tensorLayout = get_TensorLayout(outputs[i].properties.layout)

        if len(outputs[i].properties.scale_data) == 0:
            output_tensors[i].properties.quantiType = 0
            output_tensors[i].sysMem[0].virAddr = ctypes.cast(
                outputs[i].buffer.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                ctypes.c_void_p
            )
        else:
            output_tensors[i].properties.quantiType = 2
            output_tensors[i].properties.scale.scaleData = outputs[i].properties.scale_data.ctypes.data_as(
                ctypes.POINTER(ctypes.c_float)
            )
            output_tensors[i].sysMem[0].virAddr = ctypes.cast(
                outputs[i].buffer.ctypes.data_as(ctypes.POINTER(ctypes.c_int8)),
                ctypes.c_void_p
            )

        for j in range(len(outputs[i].properties.shape)):
            output_tensors[i].properties.validShape.dimensionSize[j] = outputs[i].properties.shape[j]
        output_tensors[i].properties.validShape.numDimensions = len(outputs[i].properties.shape)

        do_process_one_output(
            ctypes.pointer(output_tensors[i]),
            ctypes.pointer(yolov5_postprocess_info),
            i
        )

    result_str = get_Postprocess_result(ctypes.pointer(yolov5_postprocess_info))
    result_str = result_str.decode('utf-8')
    data = json.loads(result_str[16:])
    print(f"检测到目标数量: {len(data)}")

    for result in data:
        bbox = result['bbox']
        score = result['score']
        id = result['id']
        name = result['name']
        print(f"bbox: {bbox}, score: {score:.3f}, id: {id}, name: {name}")
        cv2.rectangle(img_file, (int(bbox[0]), int(bbox[1])), (int(bbox[2]), int(bbox[3])), (0, 255, 0), 2)
        font = cv2.FONT_HERSHEY_SIMPLEX
        cv2.putText(img_file, f'{name} {score:.2f}', (int(bbox[0]), int(bbox[1]) - 10), font, 0.5, (0, 255, 0), 1)

    cv2.imwrite('output_car_1.jpg', img_file)
    print("结果图片已保存: output_image.jpg")