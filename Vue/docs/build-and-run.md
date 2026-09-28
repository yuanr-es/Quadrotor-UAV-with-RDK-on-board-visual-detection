# 构建与运行指南（uni-app x）

本工程已从 Web 原型转换为 **uni-app x** 工程（根目录含 `App.uvue`、`main.uts`、
`manifest.json`（带 `uni-app-x` 节点）、`pages.json`）。本机已安装 **HBuilderX 5.07**。

## 1. 前置条件

- HBuilderX 5.07（本机路径：`E:\迅雷下载\HBuilderX.5.07.2026041006\HBuilderX`）
- 目标 Android 手机开启 USB 调试（首次运行到真机需 HBuilderX 自动安装“真机运行”插件）
- 云打包生成 APK 需要：DCloud 账号 + 本项目 `manifest.json` 中填写有效的 DCloud `appid`
- 本地离线打包（可选）需要：Android SDK/NDK、JDK、App 离线 SDK

> 当前 `manifest.json` 的 `appid` 为空。请用 HBuilderX 打开项目后，
> 通过 `manifest.json` 可视化界面申请/填入 DCloud appid，否则无法云打包。

## 2. 用 HBuilderX 打开项目

1. 打开 HBuilderX → `文件` → `打开目录` → 选择本工程根目录 `E:\UniApp_prj\my-uni-project`。
2. 首次打开会提示安装 uni-app x 编译器/真机运行等插件，按提示安装。
3. 确认项目图标为圆形（表示被识别为 uni-app x 项目）；若为方形，检查 `manifest.json` 是否含 `"uni-app-x": {}`。

## 3. 运行到 Android 真机（调试）

1. 手机用数据线连接，开启 USB 调试。
2. HBuilderX 菜单 `运行` → `运行到手机或模拟器` → 选择设备。
3. 选择 **标准基座**（HBuilderX 3.97+ 标准基座已内置全部 Android 权限，便于早期调试）。
4. 若需自定义基座（含自有插件/权限），先 `发行`→`原生 App-云打包`→ 生成自定义调试基座并安装。

## 4. 生成 APK（云打包）

1. `发行` → `原生 App-云打包`。
2. 选择 Android，填写包名、证书（可用 DCloud 公共测试证书或自有证书）。
3. `manifest.json` → `app-android` → `distribute` → `permissions` 中声明的权限会在此生效。
4. 打包完成后下载 APK，`adb install` 或手机直接安装。

> 当前仅声明：USB Host feature、蓝牙（含 Android 12+ `BLUETOOTH_SCAN/CONNECT`）、定位。
> USB 访问授权由运行时 `UsbManager.requestPermission()` 完成，不使用伪造的 `USB_PERMISSION`。

## 5. 单元测试（与 App 构建无关）

工程根 `package.json` 仅用于测试工具链（vitest），不参与 App 构建：

```bash
npm install
npm test        # 47 个测试：协议解析 / store / 状态机 / 坐标转换
```

## 6. 当前进度与限制

- 已迁移页面：`pages/index/index.uvue`（静态空壳，仅内置组件，占位显示 `--`）。
- 未迁移：`legacy-web/components/*.vue`（原 Web 组件，待按 §4.2 迁移为 `.uvue`）。
- 未接入：USB/UVC 视频、USB 遥测、BLE 真机链路；`domain/`、`services/` 为已测试的
  TypeScript 参考实现，需按 UTS 强类型约束移植（见 `docs/adr/ADR-001-tech-route.md`）。

## 7. UTS 迁移注意（重要）

uni-app x 的 UTS 是强类型语言，与 TS 有约束差异：

- 不支持 `undefined`，用 `null`
- 不支持 `globalThis`、命名空间
- 对象字面量默认为 `UTSJSONObject`；带类型的对象需用 `type` 定义
- `interface` 不能用于对象字面量构造，需改为 `type`

因此 `domain/`、`services/` 下的 TS 代码不能直接 `.ts` 复用，需在后续任务中改写为 `.uts`。
