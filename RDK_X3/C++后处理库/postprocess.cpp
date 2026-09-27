#include "postprocess.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>


struct Box {
    float x1, y1, x2, y2, score;
    int cls;
};

static std::vector<Box> g_box_list;
static std::string g_json_result;


static const int NUM_CLASS = 3;
static const int NUM_ANCHOR = 3;
static const int STRIDES[3] = {8, 16, 32};
static const float ANCHORS[3][6] = {
    {10.0f, 13.0f,  16.0f, 30.0f,  33.0f, 23.0f},   // stride=8  84x84
    {30.0f, 61.0f,  62.0f, 45.0f,  59.0f, 119.0f},  // stride=16 42x42
    {116.0f,90.0f, 156.0f,198.0f, 373.0f,326.0f}    // stride=32 21x21
};
static const char* CLASS_NAMES[3] = {"red_ball", "person", "car"};


static inline float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

static float iou(const Box& a, const Box& b) {
    float x1 = std::max(a.x1, b.x1);
    float y1 = std::max(a.y1, b.y1);
    float x2 = std::min(a.x2, b.x2);
    float y2 = std::min(a.y2, b.y2);
    if (x1 >= x2 || y1 >= y2) return 0.0f;
    float inter = (x2 - x1) * (y2 - y1);
    float area_a = (a.x2 - a.x1) * (a.y2 - a.y1);
    float area_b = (b.x2 - b.x1) * (b.y2 - b.y1);
    return inter / (area_a + area_b - inter);
}

static bool score_compare(const Box& a, const Box& b) {
    return a.score > b.score;
}

static float clipf(float v, float minv, float maxv) {
    if (v < minv) return minv;
    if (v > maxv) return maxv;
    return v;
}


extern "C" void Yolov5doProcess(hbDNNTensor_t* tensor,
                                Yolov5PostProcessInfo_t* info,
                                int stride_idx) {
    if (!tensor || !info) return;
    if (stride_idx < 0 || stride_idx >= 3) return;

    void* data_ptr = tensor->sysMem[0].virAddr;
    if (!data_ptr) return;

    // NHWC: [N, H, W, C]  C = 3*(5+NUM_CLASS)
    int h = tensor->properties.validShape.dimensionSize[1];
    int w = tensor->properties.validShape.dimensionSize[2];
    int c = tensor->properties.validShape.dimensionSize[3];

    int stride = STRIDES[stride_idx];
    const float* anchors = ANCHORS[stride_idx];
    int per_anchor = 5 + NUM_CLASS; // x,y,w,h,obj + classes

    float score_thr = info->score_threshold;
    int quanti_type = tensor->properties.quantiType;

    if (quanti_type == 0) {
        float* data = reinterpret_cast<float*>(data_ptr);
        for (int ay = 0; ay < h; ay++) {
            for (int ax = 0; ax < w; ax++) {
                // 当前grid点起始地址 NHWC
                float* grid_base = data + (ay * w + ax) * c;

                for (int k = 0; k < NUM_ANCHOR; k++) {
                    float* cur = grid_base + k * per_anchor;

                    float tx = cur[0];
                    float ty = cur[1];
                    float tw = cur[2];
                    float th = cur[3];
                    float obj = cur[4];

                    float obj_conf = sigmoid(obj);
                    if (obj_conf < score_thr) continue;

                    float max_cls = -1e9f;
                    int cls_id = 0;
                    for (int ci = 0; ci < NUM_CLASS; ci++) {
                        float s = cur[5 + ci];
                        if (s > max_cls) {
                            max_cls = s;
                            cls_id = ci;
                        }
                    }
                    float cls_conf = sigmoid(max_cls);
                    float final_score = obj_conf * cls_conf;
                    if (final_score < score_thr) continue;

                    float bx = (sigmoid(tx) * 2.0f - 0.5f + ax) * stride;
                    float by = (sigmoid(ty) * 2.0f - 0.5f + ay) * stride;
                    float bw = powf(sigmoid(tw) * 2.0f, 2.0f) * anchors[k * 2 + 0];
                    float bh = powf(sigmoid(th) * 2.0f, 2.0f) * anchors[k * 2 + 1];

                    Box box;
                    box.x1 = bx - bw * 0.5f;
                    box.y1 = by - bh * 0.5f;
                    box.x2 = bx + bw * 0.5f;
                    box.y2 = by + bh * 0.5f;
                    box.score = final_score;
                    box.cls = cls_id;
                    g_box_list.push_back(box);
                }
            }
        }
        return;
    }

    if (quanti_type == 2) {
        int8_t* data = reinterpret_cast<int8_t*>(data_ptr);
        float* scale_ptr = tensor->properties.scale.scaleData;
        if (!scale_ptr) return;

        for (int ay = 0; ay < h; ay++) {
            for (int ax = 0; ax < w; ax++) {
                int8_t* grid_base = data + (ay * w + ax) * c;

                for (int k = 0; k < NUM_ANCHOR; k++) {
                    int8_t* cur = grid_base + k * per_anchor;
                    float* s_cur = scale_ptr + k * per_anchor;

                    float tx = cur[0] * s_cur[0];
                    float ty = cur[1] * s_cur[1];
                    float tw = cur[2] * s_cur[2];
                    float th = cur[3] * s_cur[3];
                    float obj = cur[4] * s_cur[4];

                    float obj_conf = sigmoid(obj);
                    if (obj_conf < score_thr) continue;

                    float max_cls = -1e9f;
                    int cls_id = 0;
                    for (int ci = 0; ci < NUM_CLASS; ci++) {
                        float s = cur[5 + ci] * s_cur[5 + ci];
                        if (s > max_cls) {
                            max_cls = s;
                            cls_id = ci;
                        }
                    }
                    float cls_conf = sigmoid(max_cls);
                    float final_score = obj_conf * cls_conf;
                    if (final_score < score_thr) continue;

                    float bx = (sigmoid(tx) * 2.0f - 0.5f + ax) * stride;
                    float by = (sigmoid(ty) * 2.0f - 0.5f + ay) * stride;
                    float bw = powf(sigmoid(tw) * 2.0f, 2.0f) * anchors[k * 2 + 0];
                    float bh = powf(sigmoid(th) * 2.0f, 2.0f) * anchors[k * 2 + 1];

                    Box box;
                    box.x1 = bx - bw * 0.5f;
                    box.y1 = by - bh * 0.5f;
                    box.x2 = bx + bw * 0.5f;
                    box.y2 = by + bh * 0.5f;
                    box.score = final_score;
                    box.cls = cls_id;
                    g_box_list.push_back(box);
                }
            }
        }
        return;
    }
}

extern "C" const char* Yolov5PostProcess(Yolov5PostProcessInfo_t* info) {
    g_json_result.clear();

    if (!info) {
        g_json_result = std::string(16, ' ') + "[]";
        return g_json_result.c_str();
    }

    std::sort(g_box_list.begin(), g_box_list.end(), score_compare);
    std::vector<Box> nms_out;
    std::vector<bool> suppressed(g_box_list.size(), false);

    for (size_t i = 0; i < g_box_list.size(); i++) {
        if (suppressed[i]) continue;
        if ((int)nms_out.size() >= info->nms_top_k) break;
        nms_out.push_back(g_box_list[i]);

        for (size_t j = i + 1; j < g_box_list.size(); j++) {
            if (suppressed[j]) continue;
            if (g_box_list[i].cls != g_box_list[j].cls) continue;
            if (iou(g_box_list[i], g_box_list[j]) > info->nms_threshold) {
                suppressed[j] = true;
            }
        }
    }
    g_box_list.clear(); 

    int in_w = info->width;
    int in_h = info->height;
    int ori_w = info->ori_width;
    int ori_h = info->ori_height;

    float scale_x = 1.0f, scale_y = 1.0f;
    float dw = 0.0f, dh = 0.0f;

    if (info->is_pad_resize == 1) {
        // letterbox模式：等比例缩放+padding
        float scale = std::min((float)in_w / ori_w, (float)in_h / ori_h);
        scale_x = scale;
        scale_y = scale;
        dw = (in_w - ori_w * scale) * 0.5f;
        dh = (in_h - ori_h * scale) * 0.5f;
    } else {
        // 直接resize模式：x和y分别缩放
        scale_x = (float)ori_w / in_w;
        scale_y = (float)ori_h / in_h;
    }

    // 拼接JSON
    std::string json_body = "[";
    for (size_t i = 0; i < nms_out.size(); i++) {
        Box& b = nms_out[i];
        float x1, y1, x2, y2;

        if (info->is_pad_resize == 1) {
            x1 = (b.x1 - dw) / scale_x;
            y1 = (b.y1 - dh) / scale_y;
            x2 = (b.x2 - dw) / scale_x;
            y2 = (b.y2 - dh) / scale_y;
        } else {
            x1 = b.x1 * scale_x;
            y1 = b.y1 * scale_y;
            x2 = b.x2 * scale_x;
            y2 = b.y2 * scale_y;
        }

        x1 = clipf(x1, 0.0f, (float)(ori_w - 1));
        y1 = clipf(y1, 0.0f, (float)(ori_h - 1));
        x2 = clipf(x2, 0.0f, (float)(ori_w - 1));
        y2 = clipf(y2, 0.0f, (float)(ori_h - 1));

        char buf[512] = {0};
        snprintf(buf, sizeof(buf),
            "{\"bbox\":[%.2f,%.2f,%.2f,%.2f],\"score\":%.3f,\"id\":%d,\"name\":\"%s\"}",
            x1, y1, x2, y2, b.score, b.cls, CLASS_NAMES[b.cls]);

        json_body += buf;
        if (i != nms_out.size() - 1) json_body += ",";
    }
    json_body += "]";

    std::string header(16, ' ');
    g_json_result = header + json_body;
    return g_json_result.c_str();
}

