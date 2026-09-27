#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hbSysMem_t {
    double phyAddr;
    void* virAddr;
    int memSize;
} hbSysMem_t;

typedef struct hbDNNQuantiShift_yt {
    int shiftLen;
    char* shiftData;
} hbDNNQuantiShift_yt;

typedef struct hbDNNQuantiScale_t {
    int scaleLen;
    float* scaleData;
    int zeroPointLen;
    char* zeroPointData;
} hbDNNQuantiScale_t;

typedef struct hbDNNTensorShape_t {
    int dimensionSize[8];
    int numDimensions;
} hbDNNTensorShape_t;

typedef struct hbDNNTensorProperties_t {
    hbDNNTensorShape_t validShape;
    hbDNNTensorShape_t alignedShape;
    int tensorLayout;
    int tensorType;
    hbDNNQuantiShift_yt shift;
    hbDNNQuantiScale_t scale;
    int quantiType;
    int quantizeAxis;
    int alignedByteSize;
    int stride[8];
} hbDNNTensorProperties_t;

typedef struct hbDNNTensor_t {
    hbSysMem_t sysMem[4];
    hbDNNTensorProperties_t properties;
} hbDNNTensor_t;

typedef struct Yolov5PostProcessInfo_t {
    int height;
    int width;
    int ori_height;
    int ori_width;
    float score_threshold;
    float nms_threshold;
    int nms_top_k;
    int is_pad_resize;
} Yolov5PostProcessInfo_t;

void Yolov5doProcess(hbDNNTensor_t* tensor, Yolov5PostProcessInfo_t* info, int stride_idx);
const char* Yolov5PostProcess(Yolov5PostProcessInfo_t* info);

#ifdef __cplusplus
}
#endif

#endif

