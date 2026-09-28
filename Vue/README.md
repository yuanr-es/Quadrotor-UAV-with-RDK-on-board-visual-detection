# 无人机控制端（uni-app x）

本工程目标：可安装、可连接真实设备的 Android / HarmonyOS 无人机地面控制端。
详细规划见 `TODOList.md`，技术路线决策见 `docs/adr/ADR-001-tech-route.md`。

> 当前状态：**已转换为 uni-app x 工程（T05 进行中）**，含一个静态空壳页面。
> 真实 USB/UVC 视频、USB 遥测、BLE 控制链路尚未接入（受真实硬件与协议阻塞）。

## 工程类型

本项目是 **uni-app x** 工程，用 **HBuilderX** 打开、运行、打包：

- 入口：`main.uts` + `App.uvue`
- 标识：`manifest.json` 中的 `"uni-app-x": {}`
- 页面：`pages/index/index.uvue`
- 根目录 `package.json` **仅用于单元测试工具链**（vitest），不参与 App 构建

构建/运行步骤见 `docs/build-and-run.md`。

## 环境版本（本机已验证）

| 工具 | 版本 |
|---|---|
| HBuilderX | 5.07.2026041006 |
| Node | v24.15.0 |
| npm | 11.12.1 |
| Git | 2.54.0 |
| JDK | 17.0.12 |

## 常用命令

```bash
npm install   # 仅安装测试依赖
npm test      # 运行 47 个单元测试
```

> Web 调试入口（`npm run dev`）已随迁移移除；原 Web 原型保存在 Git 历史（commit `ea38ec1`）。

## 目录结构

```text
App.uvue / main.uts         uni-app x 入口
manifest.json / pages.json  工程配置（含 app-android 权限）
platformConfig.json         编译目标：APP-ANDROID
pages/
  index/index.uvue          控制器主页（仪表盘 + 导航）
  devices/ video/ control/ diagnostics/ about/   功能分页
components/
  top-bar/ status-panel/ status-card/ video-overlay/ joystick/ bluetooth-panel/   （easycom，.uvue）
domain/                     领域层（TS 参考实现，待按 UTS 约束移植）
  video/                    设备模型、会话状态机、坐标转换、适配器接口
  telemetry/                遥测领域模型与 store
services/                   应用/服务层（TS 参考实现）
  telemetry-parser.ts       二进制分帧与 CRC（机制，协议常量待定）
  telemetry-transport.ts    传输抽象 + 回放传输
  telemetry-service.ts      transport -> parser -> decoder -> store 编排
  dev-mock.ts               开发 Mock（显式开关）
  diagnostics.ts            环形日志 / 计数 / 导出（脱敏）
uni_modules/usb-uvc-video/utssdk/
  interface.uts             跨平台统一接口声明
  app-android/index.uts     Android 实现骨架（未实现，显式抛错）
  app-harmony/index.uts     HarmonyOS 实现骨架（未实现，显式抛错）
legacy-web/                 原 Web 组件（已迁移，保留仅供参照）
tests/                      协议解析 / store / 状态机 / 坐标转换单测
docs/                       ADR、协议、硬件矩阵、发布清单、构建指南、报告模板
```

## 已完成

- [x] T01 Git 基线
- [x] T04 ADR-001 技术路线（UniApp X + UTS）
- [x] T06 `UsbVideoAdapter` 接口 + 视频会话状态机
- [x] T09 删除随机遥测，改为显式开发 Mock
- [x] T10 遥测 parser / transport / store 拆分 + 单元测试
- [x] §6 目标框坐标转换 + 单元测试
- [x] T12 诊断面板 + 测试报告模板
- [x] §4.2 UI 迁移：5 个功能页面 + 6 个 `.uvue` 组件（easycom），页面生命周期改用 `onLoad/onReady`
- [ ] T05 工程转为 uni-app x：模板已建、静态页已迁移，**空壳 APK 待 HBuilderX 云打包生成**

## 未完成（受硬件/协议阻塞）

- USB/UVC 真实视频链路（§5）、USB 遥测链路（§7）、BLE 真机链路（§8）
- domain/services 的 UTS 移植（当前为已测试的 TS 参考实现）
- HarmonyOS 原生 HAP（§11，需 DevEco Studio 与鸿蒙 POC）

## 文档

- `TODOList.md`：总规划与里程碑
- `docs/build-and-run.md`：HBuilderX 构建与运行指南
- `docs/adr/ADR-001-tech-route.md`：技术路线
- `docs/protocol.md`：设备协议（待真实数据填充）
- `docs/hardware-matrix.md`：硬件兼容矩阵（待真机验证）
- `docs/release-checklist.md`：发布检查清单
- `docs/technical-report-template.md`：真机测试报告模板
