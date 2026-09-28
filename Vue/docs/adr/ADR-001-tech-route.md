# ADR-001：无人机控制端技术路线选型

- 状态：Accepted（已采纳）
- 日期：2026-09-27
- 决策人：项目组
- 关联：TODOList.md §3.1、§3.2、§11

## 背景（Context）

当前工程 `my-uni-project` 只是一个 Vue 3 + Vite 的 **Web 演示原型**：

- 没有 `@dcloudio/*` 编译器，用 `createApp().mount('#app')` 启动；
- `rpx` 通过自制正则插件转换为 `vw`；
- USB 视频、遥测均为占位或随机数据，`manifest.json` 里的权限声明不生效。

产品目标是在 **Android 真机** 上打通「USB 摄像头预览 + USB 遥测 + BLE 控制」闭环；同时需要明确 **HarmonyOS** 的落地边界。可选路线有三：

1. **UniApp X + Vue/UTS 跨端接口 + 平台原生实现**（规划推荐）
2. 标准 UniApp Vue 3 + Android 原生插件
3. 纯原生 Android（Kotlin/Compose 或 View）

## 决策（Decision）

采用 **路线 1：UniApp X + Vue/UTS 跨端接口 + 平台原生实现**。

- UI 层：Vue 3（UniApp X 的 `.uvue` 页面 / 组件）。
- 公共业务层：TypeScript/UTS 编写的领域与服务模块（transport / parser / store / 状态机 / 坐标转换），保持平台无关、可单元测试。
- 视频与设备层：通过 `uni_modules/usb-uvc-video` 的 UTS 接口声明统一 API；
  - `app-android` 使用 Android USB Host + UVC（Surface/MediaCodec）；
  - `app-harmony` 使用 ArkTS / 鸿蒙 USB 与媒体能力；
- 开发 Mock 实现仅在 development 环境显式启用，发布构建编译关闭。

## 备选方案对比

| 维度 | UniApp X + UTS | 标准 UniApp + Android 插件 | 纯原生 Android |
|---|---|---|---|
| 现有 Vue UI 复用 | 高（需迁移为 uvue） | 高 | 低（需重写） |
| Android 低延迟视频 | 中高（UTS 原生组件） | 中高 | 高 |
| HarmonyOS NEXT 复用 | 好（同一 UTS 接口 + ArkTS 实现） | 差（需独立工程） | 差（独立工程） |
| 团队学习成本 | 中（UTS/uvue） | 低 | 高（若团队偏前端） |
| 长期双端维护成本 | 低 | 中 | 高 |

## 后果（Consequences）

**正面**
- 一套 UTS 接口约束 Android 与 HarmonyOS 两个平台实现，避免逻辑重复。
- 领域/服务层为纯 TS，可脱离真机做高覆盖单元测试（协议解析、坐标转换、状态机）。
- 保留 Vue 技术栈，UI 迁移成本可控。

**负面 / 风险**
- 需引入 HBuilderX / uni-app x CLI、UTS 与 uvue 的学习成本。
- UTS 原生组件同层渲染、Surface 嵌入需在真机验证（见 TODOList §5.4、§11.2）。
- 鸿蒙侧 USB/UVC 公开能力若不足，需厂商 HAR/SDK，存在范围风险（见 ADR-002 待定）。

## 待办 / 后续 ADR

- ADR-002（待定）：HarmonyOS 路线 A（APK 兼容）vs 路线 B（原生 HAP）的最终裁决，需先完成 §11.2 的 UVC POC。
- ADR-003（待定）：UVC 第三方库（libuvc / AndroidUSBCamera / 自研）的许可证与 ABI 决策，需真实硬件 POC 后锁定。

## 参考

- DCloud UTS 插件：https://uniapp.dcloud.net.cn/plugin/uts-plugin.html
- DCloud UTS Android 原生能力：https://uniapp.dcloud.net.cn/plugin/uts-for-android.html
- DCloud UTS 原生组件：https://uniapp.dcloud.net.cn/plugin/uts-component.html
- DCloud 鸿蒙原生组件嵌入：https://uniapp.dcloud.net.cn/tutorial/harmony/native-component.html
- Android USB Host：https://developer.android.com/develop/connectivity/usb/host
