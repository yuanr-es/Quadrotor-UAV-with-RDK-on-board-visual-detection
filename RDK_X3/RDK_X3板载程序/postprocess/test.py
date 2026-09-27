import ctypes

# 结构体必须和C代码定义完全对齐
class DetectObject(ctypes.Structure):
    _fields_ = [
        ("x1", ctypes.c_float),
        ("y1", ctypes.c_float),
        ("x2", ctypes.c_float),
        ("y2", ctypes.c_float),
        ("score", ctypes.c_float),
        ("cls", ctypes.c_int)
    ]

lib_path = "./build/libpostprocess.so"
lib = ctypes.CDLL(lib_path)

# 设置函数入参、返回值类型
lib.yolov5_postprocess.argtypes = [
    ctypes.POINTER(ctypes.c_float),
    ctypes.POINTER(ctypes.c_float),
    ctypes.POINTER(ctypes.c_float),
    ctypes.c_float,
    ctypes.c_float,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.POINTER(DetectObject),
    ctypes.c_int
]
lib.yolov5_postprocess.restype = ctypes.c_int

print("库加载成功！函数符号正常识别")