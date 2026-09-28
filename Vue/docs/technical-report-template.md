# 真机测试报告模板

> 每次发布或里程碑验收填写本文件，归档到 `docs/reports/<版本>-<日期>.md`。

## 基本信息

| 项 | 值 |
|---|---|
| 版本 / versionCode | TODO |
| Git commit | TODO |
| 构建类型 | Debug / Release |
| 构建日期 | TODO |
| 测试人 | TODO |

## 测试设备

| 机型 | 系统版本 | ABI | USB 摄像头 | 固件 | OTG/Hub |
|---|---|---|---|---|---|
| TODO | TODO | TODO | TODO | TODO | TODO |

## 连接拓扑

（画出视频 / 遥测 / 控制分别走 USB / BLE / Wi-Fi 的拓扑）

## 用例与结果

| 用例 | 步骤 | 期望 | 实际 | 结论 |
|---|---|---|---|---|
| 冷启动已插设备 | TODO | 自动识别并请求授权 | TODO | PASS/FAIL |
| 启动后插入 | TODO | 弹出授权并预览 | TODO | |
| 播放中拔出 | TODO | 停止读请求并释放资源，UI 提示 | TODO | |
| 快速连续插拔 | TODO | 无双重 open/close | TODO | |
| 授权拒绝后重试 | TODO | 可再次请求，不循环弹窗 | TODO | |
| 前后台切换 | TODO | 正确释放/恢复 | TODO | |
| BLE 超距断线 | TODO | 进入安全状态并告警 | TODO | |
| 30 分钟稳定预览 | TODO | 无崩溃 / 无内存增长 | TODO | |

## 性能数据

| 指标 | 数值 | 方法 |
|---|---|---|
| 首帧时间 P50 / P95 | TODO | TODO |
| 端到端视频延迟 | TODO | TODO |
| 采集 FPS / 渲染 FPS | TODO | TODO |
| 丢帧率 | TODO | TODO |
| 最大连续卡顿 | TODO | TODO |
| 控制发送抖动 / ACK 时延 | TODO | TODO |
| 失联检测时间 | TODO | TODO |
| CPU / 内存 / 温度 / 电量 | TODO | TODO |

## 问题清单

| 编号 | 严重级别 | 描述 | 复现步骤 | 状态 |
|---|---|---|---|---|
| BUG-001 | 阻断/严重/一般 | TODO | TODO | open/closed |

## 结论

- [ ] 通过验收
- [ ] 有条件通过（列明条件）
- [ ] 不通过

## 附件

- [ ] `adb logcat` 日志
- [ ] 原生崩溃栈
- [ ] 诊断页导出数据
