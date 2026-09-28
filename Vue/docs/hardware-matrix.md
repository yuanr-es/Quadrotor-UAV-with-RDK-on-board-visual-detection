# 硬件兼容矩阵（待真机验证填充）

> 状态：**模板 / 待填充**。参见 TODOList.md §2.3、§12.2。

## 1. 目标 Android 设备

| 编号 | 品牌/机型 | 系统版本 | ABI | OTG | USB Host 枚举 | 供电能力 | 解码器 | 热插拔 | 结论 |
|---|---|---|---|---|---|---|---|---|---|
| D1 主力机 | TODO | TODO | TODO | TODO | TODO | TODO | TODO | TODO | TODO |
| D2 低性能机 | TODO | TODO | TODO | TODO | TODO | TODO | TODO | TODO | TODO |
| D3 华为目标机 | TODO | TODO | TODO | TODO | TODO | TODO | TODO | TODO | TODO |

## 2. USB 视频设备

| 编号 | 型号 | VID | PID | 固件 | 格式 | 最高稳定配置 | 已知限制 |
|---|---|---|---|---|---|---|---|
| C1 | TODO | TODO | TODO | TODO | TODO | TODO | TODO |

## 3. HarmonyOS 设备

| 编号 | 机型 | 系统代际（NEXT / 兼容 APK） | USB Host | UVC | 结论 |
|---|---|---|---|---|---|
| H1 | TODO | TODO | TODO | TODO | TODO |

## 4. 记录规则

- 系统版本必须记录准确版本号，不得只写营销名「鸿蒙」。
- 视频配置以「连续预览 10 分钟无崩溃」为最低稳定门槛。
- 每台设备需记录：OTG 支持、USB Host 枚举结果、持续供电能力、可用解码器、热插拔表现。
