// Copyright (c) 2020 Horizon Robotics.All Rights Reserved.
// 整合精简版 YOLOv5 PTQ后处理 独立源码
// ctypes友好：移除std::string，仅输出id、坐标、置信度
#include <arm_neon.h>
#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <cmath>
#include <limits>
#include "hbdnn.h"

// ====================== 【整合 method_data 结构体定义】 ======================
struct Bbox {
  float x1;
  float y1;
  float x2;
  float y2;
  Bbox() : x1(0), y1(0), x2(0), y2(0) {}
  Bbox(float xmin, float ymin, float xmax, float ymax)
      : x1(xmin), y1(ymin), x2(xmax), y2(ymax) {}
};

// 移除std::string，ctypes兼容
struct Detection {
  int id;
  float score;
  Bbox bbox;
  Detection(int id_, float score_, Bbox bbox_)
      : id(id_), score(score_), bbox(bbox_) {}
};

struct ImageTensor {
  int width_;
  int height_;
  int ori_width_;
  int ori_height_;
  bool is_pad_resize;

  int width() const { return width_; }
  int height() const { return height_; }
  int ori_width() const { return ori_width_; }
  int ori_height() const { return ori_height_; }
};

struct Perception {
  enum Type { DET };
  Type type;
  std::vector<Detection> det;
};

// ====================== 【整合 utils algorithm / nms】 ======================
static int argmax(const float* start, const float* end) {
  float max_val = -1e8f;
  int idx = 0;
  int cur_idx = 0;
  for (; start != end; ++start, cur_idx++) {
    if (*start > max_val) {
      max_val = *start;
      idx = cur_idx;
    }
  }
  return idx;
}

static float iou(const Bbox& a, const Bbox& b) {
  float inter_x1 = std::max(a.x1, b.x1);
  float inter_y1 = std::max(a.y1, b.y1);
  float inter_x2 = std::min(a.x2, b.x2);
  float inter_y2 = std::min(a.y2, b.y2);
  if (inter_x1 >= inter_x2 || inter_y1 >= inter_y2) return 0.f;
  float inter_area = (inter_x2 - inter_x1) * (inter_y2 - inter_y1);
  float area_a = (a.x2 - a.x1) * (a.y2 - a.y1);
  float area_b = (b.x2 - b.x1) * (b.y2 - b.y1);
  return inter_area / (area_a + area_b - inter_area);
}

static void yolo5_nms(std::vector<Detection>& dets, float nms_thresh, int topk,
                      std::vector<Detection>& out, bool agnostic) {
  out.clear();
  std::sort(dets.begin(), dets.end(),
            [](const Detection& a, const Detection& b) { return a.score > b.score; });
  std::vector<bool> suppressed(dets.size(), false);
  int keep_cnt = 0;
  for (size_t i = 0; i < dets.size(); ++i) {
    if (suppressed[i]) continue;
    if (keep_cnt >= topk) break;
    out.push_back(dets[i]);
    keep_cnt++;
    for (size_t j = i + 1; j < dets.size(); ++j) {
      if (suppressed[j]) continue;
      if (!agnostic && dets[i].id != dets[j].id) continue;
      if (iou(dets[i].bbox, dets[j].bbox) > nms_thresh) {
        suppressed[j] = true;
      }
    }
  }
}

// ====================== YOLOv5 配置结构体 ======================
struct PTQYolo5Config {
  std::vector<int> strides;
  std::vector<std::vector<std::pair<double, double>>> anchors_table;
  int class_num;
  // 移除class_names，名称交给Python映射
};

// 适配你的3类模型 red_ball,person,car
static PTQYolo5Config default_ptq_yolo5_config = {
    {8, 16, 32},
    {{{10, 13}, {16, 30}, {33, 23}},
     {{30, 61}, {62, 45}, {59, 119}},
     {{116, 90}, {156, 198}, {373, 326}}},
    3
};

#define BSWAP_32(x) static_cast<int32_t>(__builtin_bswap32(x))
#define r_int32(x, big_endian) \
  (big_endian) ? BSWAP_32((x)) : static_cast<int32_t>((x))

static float DequantiScale(int32_t data, bool big_endian, float scale_value) {
  return static_cast<float>(r_int32(data, big_endian)) * scale_value;
}

static void PostProcessSingleTensor(hbDNNTensor* tensor,
                                    ImageTensor* frame,
                                    int layer,
                                    float score_threshold,
                                    const PTQYolo5Config& yolo5_config,
                                    std::vector<Detection>& dets) {
  hbSysFlushMem(&(tensor->sysMem[0]), HB_SYS_MEM_CACHE_INVALIDATE);
  int num_classes = yolo5_config.class_num;
  int stride = yolo5_config.strides[layer];
  int num_pred = yolo5_config.class_num + 4 + 1;

  std::vector<std::pair<double, double>> &anchors = yolo5_config.anchors_table[layer];

  double h_ratio = frame->height() * 1.0 / frame->ori_height();
  double w_ratio = frame->width() * 1.0 / frame->ori_width();
  double resize_ratio = std::min(w_ratio, h_ratio);
  if (frame->is_pad_resize) {
    w_ratio = resize_ratio;
    h_ratio = resize_ratio;
  }

  int height = tensor->properties.validShape.dimensionSize[1];
  int width = tensor->properties.validShape.dimensionSize[2];
  int anchor_num = anchors.size();
  auto quanti_type = tensor->properties.quantiType;

  if (quanti_type == hbDNNQuantiType::NONE) {
    auto *data = reinterpret_cast<float *>(tensor->sysMem[0].virAddr);
    for (uint32_t h = 0; h < height; h++) {
      for (uint32_t w = 0; w < width; w++) {
        for (int k = 0; k < anchor_num; k++) {
          double anchor_x = anchors[k].first;
          double anchor_y = anchors[k].second;
          float *cur_data = data + k * num_pred;
          float objness = cur_data[4];

          int id = argmax(cur_data + 5, cur_data + 5 + num_classes);
          double x1 = 1.0 / (1.0 + std::exp(-objness));
          double x2 = 1.0 / (1.0 + std::exp(-cur_data[id + 5]));
          double confidence = x1 * x2;

          if (confidence < score_threshold) continue;

          float center_x = cur_data[0];
          float center_y = cur_data[1];
          float scale_x = cur_data[2];
          float scale_y = cur_data[3];

          double box_center_x =
              ((1.0 / (1.0 + std::exp(-center_x))) * 2 - 0.5 + w) * stride;
          double box_center_y =
              ((1.0 / (1.0 + std::exp(-center_y))) * 2 - 0.5 + h) * stride;

          double box_scale_x =
              std::pow((1.0 / (1.0 + std::exp(-scale_x))) * 2, 2) * anchor_x;
          double box_scale_y =
              std::pow((1.0 / (1.0 + std::exp(-scale_y))) * 2, 2) * anchor_y;

          double xmin = (box_center_x - box_scale_x / 2.0);
          double ymin = (box_center_y - box_scale_y / 2.0);
          double xmax = (box_center_x + box_scale_x / 2.0);
          double ymax = (box_center_y + box_scale_y / 2.0);

          double w_padding = (frame->width() - w_ratio * frame->ori_width()) / 2.0;
          double h_padding = (frame->height() - h_ratio * frame->ori_height()) / 2.0;

          double xmin_org = (xmin - w_padding) / w_ratio;
          double xmax_org = (xmax - w_padding) / w_ratio;
          double ymin_org = (ymin - h_padding) / h_ratio;
          double ymax_org = (ymax - h_padding) / h_ratio;

          if (xmax_org <= 0 || ymax_org <= 0) continue;
          if (xmin_org > xmax_org || ymin_org > ymax_org) continue;

          xmin_org = std::max(xmin_org, 0.0);
          xmax_org = std::min(xmax_org, frame->ori_width() - 1.0);
          ymin_org = std::max(ymin_org, 0.0);
          ymax_org = std::min(ymax_org, frame->ori_height() - 1.0);

          Bbox bbox(xmin_org, ymin_org, xmax_org, ymax_org);
          dets.emplace_back((int)id, confidence, bbox);
        }
        data = data + num_pred * anchors.size();
      }
    }
  } else if (quanti_type == hbDNNQuantiType::SCALE) {
    auto *data = reinterpret_cast<int32_t *>(tensor->sysMem[0].virAddr);
    auto dequantize_scale_ptr = tensor->properties.scale.scaleData;
    bool big_endian = false;
    for (uint32_t h = 0; h < height; h++) {
      for (uint32_t w = 0; w < width; w++) {
        for (int k = 0; k < anchor_num; k++) {
          double anchor_x = anchors[k].first;
          double anchor_y = anchors[k].second;
          int32_t *cur_data = data + k * num_pred;
          int offset = num_pred * k;

          float objness = DequantiScale(cur_data[4], big_endian, *(dequantize_scale_ptr + offset + 4));

          double max_cls_data = std::numeric_limits<double>::lowest();
          int id{0};
          for (int cls = 0; cls < num_classes; cls++) {
            float score = r_int32(cur_data[cls + 5], big_endian) * dequantize_scale_ptr[offset + cls + 5];
            if (score > max_cls_data) {
              max_cls_data = score;
              id = cls;
            }
          }

          double x1 = 1.0 / (1.0 + std::exp(-objness));
          double x2 = 1.0 / (1.0 + std::exp(-max_cls_data));
          double confidence = x1 * x2;
          if (confidence < score_threshold) continue;

          float center_x = DequantiScale(cur_data[0], big_endian, *(dequantize_scale_ptr + offset));
          float center_y = DequantiScale(cur_data[1], big_endian, *(dequantize_scale_ptr + offset + 1));
          float scale_x = DequantiScale(cur_data[2], big_endian, *(dequantize_scale_ptr + offset + 2));
          float scale_y = DequantiScale(cur_data[3], big_endian, *(dequantize_scale_ptr + offset + 3));

          double box_center_x =
              ((1.0 / (1.0 + std::exp(-center_x))) * 2 - 0.5 + w) * stride;
          double box_center_y =
              ((1.0 / (1.0 + std::exp(-center_y))) * 2 - 0.5 + h) * stride;

          double box_scale_x =
              std::pow((1.0 / (1.0 + std::exp(-scale_x))) * 2, 2) * anchor_x;
          double box_scale_y =
              std::pow((1.0 / (1.0 + std::exp(-scale_y))) * 2, 2) * anchor_y;

          double xmin = (box_center_x - box_scale_x / 2.0);
          double ymin = (box_center_y - box_scale_y / 2.0);
          double xmax = (box_center_x + box_scale_x / 2.0);
          double ymax = (box_center_y + box_scale_y / 2.0);

          double w_padding = (frame->width() - w_ratio * frame->ori_width()) / 2.0;
          double h_padding = (frame->height() - h_ratio * frame->ori_height()) / 2.0;

          double xmin_org = (xmin - w_padding) / w_ratio;
          double xmax_org = (xmax - w_padding) / w_ratio;
          double ymin_org = (ymin - h_padding) / h_ratio;
          double ymax_org = (ymax - h_padding) / h_ratio;

          if (xmax_org <= 0 || ymax_org <= 0) continue;
          if (xmin_org > xmax_org || ymin_org > ymax_org) continue;

          xmin_org = std::max(xmin_org, 0.0);
          xmax_org = std::min(xmax_org, frame->ori_width() - 1.0);
          ymin_org = std::max(ymin_org, 0.0);
          ymax_org = std::min(ymax_org, frame->ori_height() - 1.0);

          Bbox bbox(xmin_org, ymin_org, xmax_org, ymax_org);
          dets.emplace_back((int)id, confidence, bbox);
        }
        data = data + num_pred * anchors.size();
      }
    }
  } else {
    std::cerr << "unsupport shift dequantize!" << std::endl;
    return;
  }
}

// ====================== 【对外C接口，ctypes可直接调用】 ======================
extern "C" {
/**
 * @brief YOLOv5 PTQ后处理统一入口
 * @param tensors hbDNN输出张量数组
 * @param tensor_num 输出分支数量（YOLOv5固定3）
 * @param image_tensor 图像预处理信息
 * @param score_thresh 置信阈值
 * @param nms_thresh NMS阈值
 * @param nms_topk NMS最大保留框
 * @param out_dets 输出Detection数组（外部预分配）
 * @param max_out_num 数组最大容量
 * @return 有效检测框数量
 */
int yolov5_ptq_postprocess(hbDNNTensor* tensors,
                           int tensor_num,
                           ImageTensor* image_tensor,
                           float score_thresh,
                           float nms_thresh,
                           int nms_topk,
                           Detection* out_dets,
                           int max_out_num) {
  std::vector<Detection> dets;
  std::vector<Detection> nms_result;
  const PTQYolo5Config& cfg = default_ptq_yolo5_config;

  for (int i = 0; i < tensor_num; i++) {
    PostProcessSingleTensor(&tensors[i], image_tensor, i, score_thresh, cfg, dets);
  }
  yolo5_nms(dets, nms_thresh, nms_topk, nms_result, false);

  int copy_num = std::min((int)nms_result.size(), max_out_num);
  for (int i = 0; i < copy_num; i++) {
    out_dets[i] = nms_result[i];
  }
  return copy_num;
}
}