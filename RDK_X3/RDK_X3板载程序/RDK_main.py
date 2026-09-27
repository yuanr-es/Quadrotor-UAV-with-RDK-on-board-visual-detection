#!/usr/bin/env python3
import numpy as np
import cv2
from hobot_dnn import pyeasy_dnn as dnn
import time
import ctypes
import json
from hobot_vio import libsrcampy as srcampy
import signal
import serial

is_stop = False

def signal_handler(sig, frame):
    global is_stop
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

class PIDController:
    def __init__(self, kp, ki, kd, output_min, output_max):
        self.kp = kp
        self.ki = ki
        self.kd = kd
        self.out_min = output_min
        self.out_max = output_max

        self.integral = 0.0
        self.last_error = 0.0

    def reset(self):
        self.integral = 0.0
        self.last_error = 0.0

    def calculate(self, setpoint, feedback, dt):
        error = setpoint - feedback
        # P
        p = self.kp * error
        # I
        self.integral += error * dt
        i = self.ki * self.integral
        # D
        d = self.kd * (error - self.last_error) / dt
        self.last_error = error

        out = p + i + d
        # 限幅
        if out > self.out_max:
            out = self.out_max
        elif out < self.out_min:
            out = self.out_min
        return out

class FlightSerial:
    def __init__(self, dev="/dev/ttyS3", baud=115200):
        self.dev = dev
        self.baud = baud
        self.ser = None
        try:
            self.ser = serial.Serial(self.dev, self.baud, timeout=0.01)
            print(f"[INFO] Sucess {self.dev} {baud}")
        except Exception as e:
            print(f"[ERROR] fail {e}")

    def make_packet(self, data_bytes):
        header = bytes([0xAA, 0x55])
        tail = bytes([0xFF, 0xFF])
        return header + data_bytes + tail

    def send_control(self, roll, pitch, yaw, throttle):
        data = np.array([roll, pitch, yaw, throttle], dtype=np.float32)
        payload = data.tobytes()
        pkt = self.make_packet(payload)
        if self.ser and self.ser.is_open:
            self.ser.write(pkt)

    def recv_data(self):
        buffer = self.ser.read(1024)
        if len(buffer) < 5:
            return None
        for i in range(len(buffer) - 4):
            if buffer[i] == 0xAA and buffer[i + 1] == 0x55:
                for j in range(i + 2, len(buffer) - 1):
                    if buffer[j] == 0xFF and buffer[j + 1] == 0xFF:
                        data = buffer[i + 2: j]
                        return data
        return None

    def close(self):
        if self.ser:
            self.ser.close()

def main():
    models = dnn.load('../models/yolov5s_672x672_nv12_NHWC_simplified.bin')
    h_model, w_model = get_hw(models[0].inputs[0].properties)
    NET_W = 672
    NET_H = 672
    print_properties(models[0].inputs[0].properties)
    print("output count:", len(models[0].outputs))
    for output in models[0].outputs:
        print_properties(output.properties)

    cam = srcampy.Camera()
    DISP_W = 1920
    DISP_H = 1080
    cam.open_cam(0, -1, 30, [NET_W, DISP_W], [NET_H, DISP_H])
    disp = srcampy.Display()
    disp.display(0, DISP_W, DISP_H)
    srcampy.bind(cam, disp)

    yolov5_postprocess_info = Yolov5PostProcessInfo_t()
    yolov5_postprocess_info.height = NET_H
    yolov5_postprocess_info.width  = NET_W
    yolov5_postprocess_info.ori_height = NET_H
    yolov5_postprocess_info.ori_width  = NET_W
    yolov5_postprocess_info.score_threshold = 0.25
    yolov5_postprocess_info.nms_threshold  = 0.45
    yolov5_postprocess_info.nms_top_k = 200
    yolov5_postprocess_info.is_pad_resize = 0

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

    target_img_cx = NET_W / 2.0
    target_img_cy = NET_H / 2.0

    pid_roll  = PIDController(kp=0.002, ki=0.0001, kd=0.0005, output_min=-8.0, output_max=8.0)
    pid_pitch = PIDController(kp=0.002, ki=0.0001, kd=0.0005, output_min=-8.0, output_max=8.0)
    pid_yaw   = PIDController(kp=0.003, ki=0.0000, kd=0.0002, output_min=-10.0, output_max=10.0)

    target_area = 12000.0
    pid_throttle = PIDController(kp=0.00003, ki=0.000001, kd=0.000005, output_min=-0.2, output_max=0.2)
    base_throttle = 0.5  # 基础悬停油门，根据你的飞机修改

    TRACK_CLS = "red_ball"

    fcu = FlightSerial("/dev/ttyS3", 115200)

    scale_x = DISP_W / NET_W
    scale_y = DISP_H / NET_H
    last_time = time.time()

    global is_stop
    try:
        while not is_stop:
            img_buf = cam.get_img(2, NET_W, NET_H)
            if img_buf is None:
                continue
            nv12_data = np.frombuffer(img_buf, dtype=np.uint8)
            t0 = time.time()
            outputs = models[0].forward(nv12_data)
            t1 = time.time()
            dt = t1 - last_time
            last_time = t1
            if dt < 1e-4:
                dt = 0.001

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

            best_target = None
            max_score = 0
            for result in data:
                bbox = result['bbox']
                score = result['score']
                name = result['name']
                if name == TRACK_CLS and score > max_score:
                    max_score = score
                    best_target = result

            roll_cmd = 0.0
            pitch_cmd = 0.0
            yaw_cmd = 0.0
            throttle_cmd = base_throttle

            if best_target is not None:
                bbox = best_target['bbox']
                x1, y1, x2, y2 = bbox
                cx = (x1 + x2) / 2.0
                cy = (y1 + y2) / 2.0
                area = abs((x2-x1)*(y2-y1))

                roll_cmd = pid_roll.calculate(setpoint=target_img_cx, feedback=cx, dt=dt)
                pitch_cmd = pid_pitch.calculate(setpoint=target_img_cy, feedback=cy, dt=dt)
                yaw_cmd = pid_yaw.calculate(setpoint=target_img_cx, feedback=cx, dt=dt)

                throttle_delta = pid_throttle.calculate(setpoint=target_area, feedback=area, dt=dt)
                throttle_cmd = base_throttle + throttle_delta
            else:
                pid_roll.reset()
                pid_pitch.reset()
                pid_yaw.reset()
                pid_throttle.reset()
                roll_cmd = 0.0
                pitch_cmd = 0.0
                yaw_cmd = 0.0
                throttle_cmd = base_throttle

            fcu.send_control(roll_cmd, pitch_cmd, yaw_cmd, throttle_cmd)
            fcu_recv = fcu.recv_data()
            if fcu_recv is not None:
                print(f"[FCU RECV] {list(fcu_recv)}")

            fps = 1.0/(t1-t0)

    except Exception as e:
        print(f"\n[EXCEPTION] {e}")
    finally:
        cam.close_cam()
        disp.close()
        fcu.close()

if __name__ == '__main__':
    main()
