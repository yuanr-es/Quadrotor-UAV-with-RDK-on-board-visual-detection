# 设备协议文档（待硬件方提供真实数据）

> 状态：**模板 / 待填充**。本文件必须在拿到真实 USB 描述符与协议抓包后填写，
> 未填写前不得据此实现解析器（参见 TODOList.md §2.2、§7.2）。

## 1. 物理拓扑

| 链路 | 载体 | 说明 |
|---|---|---|
| 视频 | USB（UVC）| 待确认 |
| 遥测 | 待确认（CDC-ACM / HID / Vendor Bulk / BLE / Wi-Fi）| 待确认 |
| 控制 | 待确认（BLE / USB）| 待确认 |

## 2. USB 视频设备

| 项 | 值 |
|---|---|
| 厂商 / 型号 | TODO |
| VID | TODO |
| PID | TODO |
| 设备类 / 接口类 | TODO（确认是否标准 UVC） |
| 端点类型 / 最大包长 | TODO |
| 传输类型 | TODO（Isochronous / Bulk） |
| 支持格式 | TODO（MJPEG / YUY2 / NV12 / H.264 / H.265） |
| 分辨率×帧率 | TODO |
| 供电需求 | TODO |

## 3. 遥测二进制协议

| 字段 | 值 |
|---|---|
| 帧头 | TODO |
| 长度字段 | TODO（偏移 / 字节序） |
| 消息类型 | TODO |
| 序号 / 时间戳 | TODO |
| 校验方式 | TODO（CRC16 / 求和 / 无） |
| 转义规则 | TODO |

### 3.1 单位与取值范围

| 量 | 单位 | 范围 | 说明 |
|---|---|---|---|
| 经纬度 | TODO | TODO | |
| 高度 | TODO | TODO | |
| 水平/垂直速度 | TODO | TODO | |
| 电压 | TODO | TODO | |
| 电量 | TODO | TODO | |

## 4. 控制协议

| 项 | 值 |
|---|---|
| 通道数值范围 | TODO（如 [-1000,1000] / PWM / 角度） |
| 发送频率 | TODO（20–50 Hz 待真机确认） |
| 确认/重试机制 | TODO |
| 失联保护 | TODO |
| 紧急停机语义 | TODO |
| 目标框坐标格式 | TODO（像素 / 归一化 [0,1]，原点与旋转方向） |

## 5. BLE GATT

| 项 | 值 |
|---|---|
| Service UUID | TODO |
| 写特征 UUID | TODO（write-with / without-response） |
| 通知特征 UUID | TODO |
| 最大包长 / MTU | TODO |

## 6. 抓包样例

- [ ] 正常帧
- [ ] 断包 / 粘包
- [ ] 校验失败
- [ ] 设备重启

放置路径：`tests/fixtures/`（录制字节流，用于 parser 回放测试）。
