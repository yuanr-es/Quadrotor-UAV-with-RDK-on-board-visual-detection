#!/usr/bin/env python3

import numpy as np
import cv2
from hobot_dnn import pyeasy_dnn as dnn
import time
import ctypes
import json

from hobot_vio import libsrcampy as srcampy
import signal

is_stop = False

def signal_handler(sig, frame):
    global is_stop
    print("Exit...")
    is_stop = True

signal.signal(signal.SIGINT, signal_handler)

class hbSysMem_t(ctypes.Structure):
    _fields_ = [
        ("phyAddr",ctypes.c_double),
        ("virAddr",ctypes.c_void_p),
        ("memSize",ctypes.c_int)
    ]

class hbDNNQuantiShift_yt(ctypes.Structure):
    _fields_ = [
        ("shiftLen",ctypes.c_int),
        ("shiftData",ctypes.c_char_p)
    ]

class hbDNNQuantiScale_t(ctypes.Structure):
    _fields_ = [
        ("scaleLen",ctypes.c_int),
        ("scaleData",ctypes.POINTER(ctypes.c_float)),
        ("zeroPointLen",ctypes.c_int),
        ("zeroPointData",ctypes.c_char_p)
    ]

class hbDNNTensorShape_t(ctypes.Structure):
    _fields_ = [
        ("dimensionSize",ctypes.c_int * 8),
        ("numDimensions",ctypes.c_int)
    ]

class hbDNNTensorProperties_t(ctypes.Structure):
    _fields_ = [
        ("validShape",hbDNNTensorShape_t),
        ("alignedShape",hbDNNTensorShape_t),
        ("tensorLayout",ctypes.c_int),
        ("tensorType",ctypes.c_int),
        ("shift",hbDNNQuantiShift_yt),
        ("scale",hbDNNQuantiScale_t),
        ("quantiType",ctypes.c_int),
        ("quantizeAxis", ctypes.c_int),
        ("alignedByteSize",ctypes.c_int),
        ("stride",ctypes.c_int * 8)
    ]

class hbDNNTensor_t(ctypes.Structure):
    _fields_ = [
        ("sysMem",hbSysMem_t * 4),
        ("properties",hbDNNTensorProperties_t)
    ]


class Yolov5PostProcessInfo_t(ctypes.Structure):
    _fields_ = [
        ("height",ctypes.c_int),
        ("width",ctypes.c_int),
        ("ori_height",ctypes.c_int),
        ("ori_width",ctypes.c_int),
        ("score_threshold",ctypes.c_float),
        ("nms_threshold",ctypes.c_float),
        ("nms_top_k",ctypes.c_int),
        ("is_pad_resize",ctypes.c_int)
    ]


def get_classes():
    return np.array(["person","red_ball","car"])

libpostprocess = ctypes.CDLL('/usr/lib/libpostprocess.so')

libpostprocess.Yolov5doProcess.argtypes = [
    ctypes.POINTER(hbDNNTensor_t),
    ctypes.POINTER(Yolov5PostProcessInfo_t),
    ctypes.c_int
]
libpostprocess.Yolov5doProcess.restype = None

get_Postprocess_result = libpostprocess.Yolov5PostProcess
get_Postprocess_result.argtypes = [ctypes.POINTER(Yolov5PostProcessInfo_t)]
get_Postprocess_result.restype = ctypes.c_char_p

do_process_one_output = libpostprocess.Yolov5doProcess

def get_TensorLayout(Layout):
    if Layout == "NCHW":
        return int(2)
    else:
        return int(0)

def get_hw(pro):
    if pro.layout == "NCHW":
        return pro.shape[2], pro.shape[3]
    else:
        return pro.shape[1], pro.shape[2]

def print_properties(pro):
    print("tensor type:", pro.tensor_type)
    print("data type:", pro.dtype)
    print("layout:", pro.layout)
    print("shape:", pro.shape)


if __name__ == '__main__':
    models = dnn.load('../models/yolov5s_672x672_nv12_NHWC_simplified.bin')
    h_model, w_model = get_hw(models[0].inputs[0].properties)
    NET_W = 672
    NET_H = 672

    print_properties(models[0].inputs[0].properties)
    print("output count:", len(models[0].outputs))
    for output in models[0].outputs:
        print_properties(output.properties)

    # ========== MIPI摄像头初始化 ==========
    cam = srcampy.Camera()
    DISP_W = 1920
    DISP_H = 1080
    # 两路：推理流NET_W x NET_H；预览流DISP_W x DISP_H
    cam.open_cam(0, -1, 30, [NET_W, DISP_W], [NET_H, DISP_H])

    disp = srcampy.Display()
    disp.display(0, DISP_W, DISP_H)
    srcampy.bind(cam, disp)

    # ===== FIX 1：C后处理参数，硬件直接resize输出672x672，无letterbox =====
    yolov5_postprocess_info = Yolov5PostProcessInfo_t()
    yolov5_postprocess_info.height = NET_H
    yolov5_postprocess_info.width  = NET_W
    # 硬件输出推理帧本身就是672*672，原图就是推理帧，不需要缩放
    yolov5_postprocess_info.ori_height = NET_H
    yolov5_postprocess_info.ori_width  = NET_W
    yolov5_postprocess_info.score_threshold = 0.25
    yolov5_postprocess_info.nms_threshold  = 0.45
    yolov5_postprocess_info.nms_top_k = 200
    yolov5_postprocess_info.is_pad_resize = 0   # 直接resize模式，无padding

    output_tensors = (hbDNNTensor_t * len(models[0].outputs))()

    for i in range(len(models[0].outputs)):
        output_tensors[i].properties.tensorLayout = get_TensorLayout(models[0].outputs[i].properties.layout)
        if len(models[0].outputs[i].properties.scale_data) == 0:
            output_tensors[i].properties.quantiType = 0
        else:
            output_tensors[i].properties.quantiType = 2
            scale_data_tmp = models[0].outputs[i].properties.scale_data.reshape(1,1,1,models[0].outputs[i].properties.shape[3])
            output_tensors[i].properties.scale.scaleData = scale_data_tmp.ctypes.data_as(ctypes.POINTER(ctypes.c_float))

        for j in range(len(models[0].outputs[i].properties.shape)):
            output_tensors[i].properties.validShape.dimensionSize[j] = models[0].outputs[i].properties.shape[j]
            output_tensors[i].properties.alignedShape.dimensionSize[j] = models[0].outputs[i].properties.shape[j]

    classes = get_classes()

    while not is_stop:
        # 获取MIPI推理流NV12
        img_buf = cam.get_img(2, NET_W, NET_H)
        if img_buf is None:
            continue
        nv12_data = np.frombuffer(img_buf, dtype=np.uint8)

        t0 = time.time()
        outputs = models[0].forward(nv12_data)
        t1 = time.time()

        # 每一帧更新buffer地址，调用C后处理解码单输出
        for i in range(len(models[0].outputs)):
            out = outputs[i]
            if output_tensors[i].properties.quantiType == 0:
                ptr = out.buffer.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
            else:
                ptr = out.buffer.ctypes.data_as(ctypes.POINTER(ctypes.c_int8))
            output_tensors[i].sysMem[0].virAddr = ctypes.cast(ptr, ctypes.c_void_p)
            do_process_one_output(ctypes.pointer(output_tensors[i]), ctypes.pointer(yolov5_postprocess_info), i)

        result_str = get_Postprocess_result(ctypes.pointer(yolov5_postprocess_info))
        result_str = result_str.decode('utf-8')
        data = json.loads(result_str[16:])

        # ===== FIX2：C库输出坐标是672×672画面坐标；如果需要映射到1920×1080预览画面，在这里Python做映射 =====
        scale_x = DISP_W / NET_W
        scale_y = DISP_H / NET_H

        for result in data:
            bbox = result['bbox']
            score = result['score']
            cls_id = int(result['id'])
            name = result['name']
            x1, y1, x2, y2 = bbox

            # 672坐标映射到HDMI预览1920*1080坐标
            x1_disp = max(0, int(x1 * scale_x))
            y1_disp = max(0, int(y1 * scale_y))
            x2_disp = min(DISP_W, int(x2 * scale_x))
            y2_disp = min(DISP_H, int(y2 * scale_y))

            print(f"Detect [{name}] score={score:.2f} net:[{x1:.1f},{y1:.1f},{x2:.1f},{y2:.1f}] disp:[{x1_disp},{y1_disp},{x2_disp},{y2_disp}]")

        fps = 1.0/(t1-t0)
        print(f"fps:{fps:.1f}\n")

    cam.close_cam()
    disp.close()
    print("resource released")