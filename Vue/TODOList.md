# 无人机控制端改造 TODO List

> 文档日期：2026-09-27  
> 目标：把当前网页演示原型改造成可安装、可连接真实设备、可稳定显示 USB 视频并完成控制与遥测闭环的 Android App；同时明确 HarmonyOS 的可行边界与后续实施路线。  
> 执行原则：先打通 Android 真机最小闭环，再补齐可靠性、测试、发布；HarmonyOS 必须按“兼容 APK”和“HarmonyOS NEXT 原生 HAP”两条路线分别验证，不能用模拟数据或网页视频冒充真机能力。

---

## 0. 当前项目审计结论（基线）

### 0.1 已确认现状

- [x] 当前 `package.json` 只有 Vue、Vite、Sass，没有 `@dcloudio/*` 编译器、UniApp CLI 或 UniApp X 工程依赖。
- [x] 当前 `main.js` 使用 `createApp(App).mount('#app')`，并把 `view/text` 手工映射为 HTML 标签；这是 Web 启动方式，不是标准 UniApp App 启动方式。
- [x] 当前 `vite.config.js` 只是普通 Vite 配置，并用正则把 `rpx` 转成 `vw`。
- [x] `npm run build` 当前只能生成 `dist/index.html` 和 Web 静态资源，不能生成 APK 或 HAP。
- [x] `uni-mock.js` 只模拟了浏览器 Toast 和 DOM 查询，不具备真机原生能力。
- [x] `VideoPlayer.vue` 仅在收到普通 `videoSrc` URL 时使用 `<video>` 播放；当前 `videoSrc` 始终为空。
- [x] 项目没有 USB 设备枚举、USB 授权、插拔监听、UVC 协商、帧读取、解码、原生 Surface/Texture 渲染代码。
- [x] `telemetry.js` 只是一个被动状态容器，没有 USB 串口/HID/Bulk 读取入口，没有分帧、校验和真实协议解析。（**已部分修复**：已拆分为 transport/parser/store，见 §7.3；真实协议解析待协议资料）
- [x] 首页每秒写入随机遥测数据；发布版若不移除会制造“已连接/正在飞行”的错误状态。（**已修复**：改为显式开关的开发 Mock，见 T09）
- [x] BLE 代码只做了扫描、连接和写固定 UUID，尚未完成服务发现、特征校验、通知订阅、MTU/分包、重连和 Android 版本化权限处理。
- [x] `Joystick.vue` 直接导入 `uni-mock.js`；`VideoPlayer.vue` 使用 `$el.querySelector/getBoundingClientRect`，两处都不应直接进入原生构建。
- [x] `manifest.json` 虽然写了若干 Android 权限，但仅声明权限不等于实现 USB/UVC；其中 `android.permission.USB_PERMISSION` 不是 Android USB Host 的标准运行时权限。
- [x] 当前目录没有 Git 仓库，缺少可回滚的工程基线。（**已修复**：T01 已建立 Git 基线）

### 0.2 当前可运行性结论

| 目标 | 当前状态 | 结论 |
|---|---|---|
| 浏览器 Web 演示 | 可构建 | 仅 UI 原型和模拟数据 |
| Android APK | 不可直接构建 | 必须迁移为正式 UniApp/UniApp X 或原生 Android 工程 |
| Android USB 摄像头 | 未实现 | 无任何真实 UVC 链路 |
| Android USB 遥测 | 未实现 | 只有状态展示，无驱动/协议接入 |
| Android BLE 控制 | 部分原型 | 未达到真机稳定控制要求 |
| 华为设备安装 APK | 未验证 | 取决于具体机型与系统版本，必须真机验证 |
| HarmonyOS NEXT HAP | 未实现 | 需要 ArkTS/鸿蒙原生能力与 HAP 构建链路 |

---

## 1. 项目完成定义（Definition of Done）

### 1.1 Android 最小可交付版（MVP）

- [ ] 能从干净环境构建出已签名的 `arm64-v8a` Android APK。
- [x] App 启动后不会自动显示伪造的飞行状态或随机遥测。（**已修复**：随机遥测改为显式开发 Mock，默认关闭）
- [ ] 支持指定的真实 USB 摄像头：识别设备、请求授权、打开视频、稳定预览、拔出后安全释放、重新插入后可恢复。
- [ ] USB 视频连续预览 30 分钟无崩溃、无持续内存增长、无明显绿屏/花屏。
- [ ] 显示实时分辨率、帧率、视频格式、丢帧数和 USB 连接状态；不得硬编码“1080P 60FPS”。
- [ ] 若遥测也经 USB 传输，能完成真实数据的接收、分帧、校验、解析、超时断开和重连。
- [ ] BLE 能完成扫描、连接、服务发现、特征校验、控制数据发送、断线提示和重连。
- [ ] 摇杆控制有固定发送频率、死区、限幅、失联保护和抬手回中策略。
- [ ] 目标框坐标能正确映射到原始视频帧坐标，兼容黑边、裁剪、旋转和不同分辨率。
- [ ] Android 关键版本和至少 3 台目标机型完成真机验收。
- [ ] 具备最小用户说明、隐私说明、版本号、图标、启动图、签名和发布包归档。

### 1.2 完整稳定版

- [ ] 自动处理 USB 插入、拔出、授权拒绝、设备占用、供电不足和异常断流。
- [ ] 前后台切换、锁屏/解锁、屏幕旋转、系统回收页面后能正确释放或恢复资源。
- [ ] 控制链路、遥测链路、视频链路彼此解耦，一条链路失败不导致其余链路无提示失效。
- [ ] 具备本地诊断日志导出能力，日志不记录敏感位置数据或长期保存原始控制指令。
- [ ] 关键协议解析、坐标转换、状态机、校验算法有自动化测试。
- [ ] 发布构建关闭 Mock、调试日志、开发服务器地址和无关权限。

### 1.3 HarmonyOS 交付定义

- [ ] 明确目标是以下哪一种，结果写入 ADR：
  - [ ] A：目标华为设备仍支持安装 Android APK，只做 APK 兼容验证。
  - [ ] B：目标为 HarmonyOS NEXT，必须产出原生 HAP。
- [ ] 若为 B：在继续完整移植前，先完成“目标鸿蒙设备 + 目标 USB 摄像头”的 UVC 预览 POC。
- [ ] HarmonyOS NEXT 版本不能复用 Android AAR/SO 后假定可用；必须提供 `app-harmony`/ArkTS 实现、鸿蒙权限与设备管理实现。

---

## 2. P0：开工前必须拿到的硬件与协议资料

> 这一阶段不完成，后续只能做“看起来像”的界面，无法保证连接真实设备。

### 2.1 USB 视频设备资料

- [ ] 提供至少 1 台最终型号摄像头/图传接收器和对应 OTG 转接线。
- [ ] 记录 USB `VID`、`PID`、设备类、接口类、端点类型、最大包长和设备序列号策略。
- [ ] 导出完整 USB 描述符，确认它是否真的是标准 UVC，而不是厂商私有协议。
- [ ] 记录摄像头支持的格式：MJPEG / YUY2 / NV12 / H.264 / H.265。
- [ ] 记录每种格式支持的分辨率、帧率和带宽组合。
- [ ] 确认 USB 传输类型：Isochronous 或 Bulk；确认目标手机 USB Host 控制器是否支持所需模式和带宽。
- [ ] 确认摄像头供电需求；若手机供电不稳，准备带供电的 OTG Hub 并纳入交付清单。
- [ ] 确认视频是否携带音频；无需求则不申请麦克风权限、不实现音频链路。
- [ ] 确认是否要求拍照、录像、码率切换、曝光/增益/对焦/PTZ 等 UVC 控制项。

### 2.2 遥测与控制协议资料

- [ ] 画清实际物理拓扑：视频、遥测、控制分别走 USB、BLE、Wi-Fi 还是同一个复合 USB 设备。
- [ ] 若 USB 同时承载视频和遥测，确认遥测接口是 CDC-ACM、HID、Vendor Bulk 还是其他私有接口。
- [ ] 提供协议文档与至少 10 分钟真实抓包，包含正常、断包、粘包、校验失败和设备重启样例。
- [ ] 明确帧头、长度、消息类型、序号、时间戳、字节序、缩放、CRC/校验和、转义规则。
- [ ] 明确控制命令的确认/重试机制、最大频率、失联保护、紧急停机语义。
- [ ] 明确 BLE Service UUID、写特征 UUID、通知特征 UUID、写入类型（有响应/无响应）和最大包长。
- [ ] 确认“框选目标”协议所需的是像素坐标、归一化坐标还是中心点/宽高，及其原点和旋转方向。

### 2.3 目标设备矩阵

- [ ] 列出最低 Android 版本、目标 SDK、主要品牌/机型、CPU ABI、屏幕比例和 USB 口规格。
- [ ] 至少准备：1 台主力机、1 台低性能机、1 台华为目标机。
- [ ] 对每台设备记录 OTG 支持、USB Host 枚举结果、持续供电能力、可用解码器和热插拔表现。
- [ ] 明确 HarmonyOS 设备的准确系统版本，不能只记录营销名称“鸿蒙”。

**阶段验收物：** `docs/hardware-matrix.md`、USB 描述符、协议文档、抓包样例、目标设备清单。

---

## 3. P0：技术路线与架构决策

### 3.1 推荐路线

- [ ] 建立 ADR-001，比较并最终选择：
  - [ ] **推荐：UniApp X + Vue/UTS 跨端接口 + 平台原生实现。** 适合希望长期同时维护 Android 和 HarmonyOS NEXT；Android 视频视图由 UTS 原生组件承载，鸿蒙由 ArkTS 原生嵌入组件承载。
  - [ ] **备选：标准 UniApp Vue 3 + Android 原生插件。** Android 上线可能更快，但原生视频组件与鸿蒙复用更受限，未来 NEXT 版本可能仍需单独实现。
  - [ ] **备选：纯原生 Android（Kotlin/Compose 或 View）。** 若首要目标是低延迟和 Android 快速量产，可降低 WebView/跨端嵌入复杂度，但现有 Vue UI 需要重写，鸿蒙仍是独立工程。
- [ ] 不在完成真实硬件 POC 前承诺具体最高分辨率、帧率或端到端延迟。
- [ ] 不把未经源码、许可证、ABI、系统版本验证的市场插件直接作为核心依赖。

### 3.2 建议分层

- [ ] UI 层：页面、状态面板、设备选择、视频 OSD、设置、诊断页面。
- [ ] 应用层：连接编排、状态机、重连策略、生命周期、控制频率调度。
- [ ] 领域层：视频会话、遥测模型、控制模型、跟踪框坐标转换、错误码。
- [ ] 平台接口层：统一声明 `UsbVideoAdapter`、`TelemetryTransport`、`ControlTransport`。
- [ ] 原生实现层：`app-android` 使用 Android USB Host/UVC；`app-harmony` 使用 ArkTS/鸿蒙设备与媒体 API。
- [ ] 模拟实现层：只在 `development` 明确启用，发布构建必须编译关闭。

### 3.3 原生插件/API 边界

- [ ] 定义设备模型：`deviceId/vendorId/productId/name/serial/interfaces`。
- [ ] 定义视频配置：`format/width/height/fps/rotation/mirror`。
- [ ] 定义统一 API：`initialize()`、`listDevices()`、`requestPermission()`、`open()`、`startPreview()`、`stopPreview()`、`close()`、`release()`。
- [ ] 定义事件：`attached`、`detached`、`permissionResult`、`opened`、`streamStarted`、`stats`、`error`、`closed`。
- [ ] 原始视频帧默认不跨 JS/UTS 桥逐帧传输；在原生层解码并直接渲染到 Surface/原生组件，避免复制和 GC 抖动。
- [ ] 只有确实需要算法处理时才暴露降频后的帧或原生纹理句柄，并记录性能代价。
- [ ] 所有方法支持幂等关闭；所有异步回调携带 session ID，防止旧会话回调污染新连接。

**阶段验收物：** ADR、模块图、接口声明、错误码表、生命周期时序图。

---

## 4. P0：把网页原型迁移成可打包的 App 工程

### 4.1 工程基线

- [x] 初始化 Git 仓库，提交当前 Web 原型基线；配置忽略 `node_modules`、构建产物、签名文件和本机配置。
- [x] 在独立分支创建官方模板工程，不要继续用普通 Vite 配置伪装 UniApp。（**已执行**：经确认改为根目录就地转换，已移除 Vite 入口）
- [x] 固定 HBuilderX/CLI、Node、JDK、Android Gradle Plugin、compile SDK、NDK 的版本并写入 `README.md`。（**部分完成**：HBuilderX 5.07、Node/npm/Git/JDK 已记录；SDK/NDK 待离线打包时补充）
- [ ] 决定使用 HBuilderX 本地构建、CLI/CI 构建还是云打包；核心 USB 插件必须能生成自定义调试基座。
- [ ] 配置正式应用 ID、Android package name、版本号规则、最低系统版本和目标系统版本。
- [ ] 建立 `dev/test/prod` 三套环境配置，禁止把测试地址和 Mock 混入生产包。

### 4.2 迁移现有 UI

- [x] 迁移 `pages.json`、页面与通用组件，保留当前视觉结构但不直接复制 Web 启动代码。（**已执行**：`pages.json` 与多页面已迁移，通用组件迁移为 `components/*/*.uvue`）
- [x] 替换当前 `main.js`，使用所选框架的标准入口和 App 生命周期。（**已执行**：`main.uts` + `App.uvue`）
- [x] 删除 `view/text -> div/span` 的手工注册。
- [ ] 删除生产入口对 `uni-mock.js` 的导入；Mock 改为接口实现，通过构建环境选择。（**部分完成**：`uni-mock.js` 已移入 `legacy-web/`，不再被 App 引用；Mock 的 UTS 接口实现待补）
- [x] 将 `Joystick.vue` 的 DOM 查询改为真正的 `uni.createSelectorQuery().in(this)` 或原生坐标事件。（**已执行**：`components/joystick` 使用 `uni.createSelectorQuery().select(#id)`）
- [x] 将 `VideoPlayer.vue` 的 `$el.querySelector()` 改为跨端组件测量接口。（**已执行**：`components/video-overlay` 使用 selector query 测量）
- [x] 将页面 `mounted/beforeUnmount` 调整为 App 页面生命周期，处理 `onShow/onHide/onUnload`。（**已执行**：使用 `onLoad`/`onReady`；后台释放待链路接入后补）
- [ ] 锁定横屏方向并适配刘海、安全区、系统导航栏、不同宽高比和折叠屏。
- [ ] 检查 `rpx`、Flex、滚动区域和触控事件在目标运行时中的实际表现，不继续依赖自制 `rpx -> vw` 插件。（**部分完成**：已改用 `px` 并移除 `rpx->vw` 插件，真机表现待验证）
- [x] 增加“设备”“视频”“控制”“诊断/日志”“关于”页面，避免所有能力堆在单页。

### 4.3 Android 空壳包验收

- [ ] 在未集成 UVC 前先生成自定义调试基座和 APK。
- [ ] 真机验证启动、横屏、触摸、前后台、返回键、深色背景和安全区。
- [ ] 用 `adb logcat` 确认无启动异常、WebView 白屏、资源缺失或 ABI 错误。
- [ ] 记录构建命令和产物路径；另一台开发机按 README 可复现构建。

**阶段验收物：** 可安装空壳 APK、可复现构建说明、迁移后的页面截图。

---

## 5. P0：Android USB/UVC 原生视频插件

### 5.1 第三方库/自研方案评估

- [ ] 对候选 UVC 实现逐项评估：源代码可得性、许可证、维护状态、Android 版本、64 位 ABI、Isochronous/Bulk、MJPEG/YUY2/H.264、热插拔、Surface 输出。
- [ ] 检查候选库是否携带 `libusb/libuvc/FFmpeg` 等原生依赖及其 LGPL/GPL/商业分发义务。
- [ ] 做最小 POC：同一目标机上完成枚举、授权、打开、预览、拔插、关闭。
- [ ] 在 POC 通过后再锁定版本和校验值；把 AAR/SO 或仓库依赖写入原生插件配置。
- [ ] 若候选库不支持目标设备格式，评估：添加格式协商/解码器、让硬件改输出格式、或更换硬件；不在 JS 层补软件逐帧解码。

### 5.2 Android USB Host 配置

- [ ] 声明 `android.hardware.usb.host`，根据产品策略决定 `required=true/false`。
- [ ] 增加 `USB_DEVICE_ATTACHED` 监听与 `res/xml/device_filter.xml`；优先按经验证的 VID/PID 精确匹配。
- [ ] 使用 `UsbManager.getDeviceList()` 枚举设备，展示未授权/已授权/占用/不支持等状态。
- [ ] 使用应用私有 action + 合规 `PendingIntent` 请求 USB 授权，正确处理 Android 新版本 flag。
- [ ] 注册并释放 attach/detach/permission 广播接收器；检查 exported 属性和生命周期，避免泄漏。
- [ ] 删除伪造的 `android.permission.USB_PERMISSION`；USB 授权由 `UsbManager.requestPermission()` 完成。
- [ ] 配置仅需要的权限和 feature，移除 `READ_LOGS`、`MOUNT_UNMOUNT_FILESYSTEMS`、`GET_ACCOUNTS`、`READ_PHONE_STATE`、`WRITE_SETTINGS` 等无业务依据权限。
- [ ] 外置 USB 摄像头不自动等同手机相机；是否保留 `CAMERA` 权限以所用 SDK 和商店合规要求为准，并做无权限场景测试。

### 5.3 UVC 会话与流协商

- [ ] 识别 VideoControl/VideoStreaming 接口和端点，不假定固定接口号。
- [ ] 读取并缓存设备支持的格式、分辨率、帧间隔；UI 只展示设备真实能力。
- [ ] 建立默认配置选择算法：优先稳定性和低延迟，再提升分辨率/帧率。
- [ ] 对 Probe/Commit、接口 claim/release、alternate setting、传输请求和超时建立明确错误处理。
- [ ] 为不同设备保存兼容性配置，但键至少包含 VID/PID/固件或描述符特征，不能只按显示名称判断。
- [ ] 限制并复用帧缓冲，建立背压和丢帧策略，禁止无界队列。
- [ ] 统计采集 FPS、渲染 FPS、丢帧、解码耗时、队列深度、USB 错误和最近一帧时间。

### 5.4 解码与渲染

- [ ] MJPEG：优先验证硬件/系统解码路径，不可用时评估原生软件解码性能。
- [ ] H.264/H.265：使用系统硬件解码器时处理 SPS/PPS、关键帧、时间戳、格式变化和解码器重建。
- [ ] YUY2/NV12：在原生层做高效色彩转换，验证 CPU/GPU 占用和发热。
- [ ] 实现原生视频组件，直接承载 `SurfaceView`/`TextureView` 或所选框架等价组件。
- [ ] 正确处理 Surface 创建、尺寸变化、销毁；Surface 不存在时停止提交渲染。
- [ ] 支持旋转、镜像、比例适配（contain/cover）并向 UI 暴露实际画面区域。
- [ ] OSD 与视频叠加需验证原生同层渲染；若普通 Vue 层无法正确覆盖，OSD 一并放到原生容器或改用支持同层的实现。

### 5.5 生命周期与恢复

- [ ] 建立状态机：`idle -> detected -> permissionPending -> opening -> streaming -> stopping -> closed/error`。
- [ ] 重复点击连接、快速插拔、授权弹窗期间退后台都不得产生双重 open/close。
- [ ] 拔出设备后立即停止读请求、解码器和渲染，释放 interface/connection/thread/buffer。
- [ ] 前台恢复时重新枚举，不持有已经失效的 `UsbDeviceConnection`。
- [ ] 断流超过阈值后进入可见错误状态；有限次数重开，禁止无限高频重试。
- [ ] App 退出或页面卸载时确保全部原生线程可终止。

### 5.6 可选视频能力（MVP 稳定后）

- [ ] 截图保存到应用媒体目录，并按 Android 分区存储规则处理。
- [ ] 录像优先复用编码流；若需重编码，测量耗电、发热和掉帧。
- [ ] 显示录像时长、剩余空间、失败原因；低空间时安全停止并封装文件。
- [ ] 若不需要录像，删除当前未实现的 `isRecording` 假状态。

**阶段验收物：** UVC POC、UTS/原生组件源码、设备能力页、30 分钟稳定性记录、许可证清单。

---

## 6. P0：视频 UI 与目标框坐标映射

- [ ] `VideoPlayer` 不再接收虚构的网页 `videoSrc` 作为 USB 主路径；改为原生预览组件 + 会话状态。
- [ ] 显示明确状态：未插入、待授权、授权拒绝、不支持、打开中、播放中、断流、已拔出。
- [ ] 分辨率和 FPS 由插件统计事件更新，不写死 `1080P 60FPS`。
- [ ] 获取四组尺寸：原始帧、旋转后帧、组件容器、实际视频显示矩形。
- [ ] 将触摸坐标先裁剪到实际视频区域，剔除 contain 模式产生的黑边。
- [ ] 根据旋转与镜像做逆变换，再转换成协议要求的像素或 `[0,1]` 归一化坐标。
- [ ] 修正当前字段名 `hx/hy` 的歧义，按协议统一为 `x/y/width/height` 或 `centerX/centerY/width/height`。
- [ ] 设置最小框大小、越界限制、取消/重画操作和发送前确认策略。
- [x] 给坐标变换编写参数化测试：4 种旋转、镜像开关、contain/cover、多种宽高比。
- [ ] 若设备回传跟踪结果，在视频层显示确认框、跟踪 ID、置信度与丢失状态。

**验收：** 在测试图卡四角和中心框选，飞控/算法侧收到的坐标误差不超过双方约定阈值。

---

## 7. P0：真实 USB 遥测链路

> 若最终确认遥测不走 USB，本节替换为对应 Wi-Fi/BLE/串口实现，但 `telemetry.js` 不能继续靠定时随机数驱动。

### 7.1 Transport 层

- [ ] 根据描述符实现 CDC-ACM、HID 或 Vendor Bulk 对应传输，不猜测设备类型。
- [ ] 若视频和遥测在同一复合设备上，统一管理一条 `UsbDeviceConnection` 或明确库间共享/独占策略。
- [ ] 配置波特率、数据位、停止位、校验位（仅 CDC 场景）或 Bulk endpoint/timeout。
- [ ] 独立读线程/协程 + 可取消 I/O；设置合理超时，不阻塞 UI 线程。
- [ ] 实现环形缓冲区、有界队列、粘包/拆包处理和流量统计。
- [ ] 拔出/异常时只通知一次断开，并能重新创建完整会话。

### 7.2 协议层

- [ ] 为协议建立二进制消息定义，禁止用未约定的 JSON 直接写硬件。
- [ ] 实现帧头搜索、长度校验、消息类型、序号、CRC、转义和非法帧恢复。
- [ ] 明确所有单位和取值范围：经纬度、厘米/米、厘米每秒/米每秒、毫伏/伏、百分比。
- [ ] 使用设备时间戳或接收时间戳，处理乱序、重复和过期数据。
- [ ] 遥测超过阈值未更新时设置 `stale/disconnected`，UI 不继续显示“飞行中”。
- [ ] 解析异常计数并限速记录日志，避免坏帧造成日志风暴。
- [ ] 使用录制的真实字节流做回放测试，不依赖 UI 手工观察。

### 7.3 状态层

- [x] 将 `telemetry.js` 拆成 transport、parser、store；store 只接收已经校验过的领域对象。
- [x] 将连接状态与飞行状态分离，不能用收到任意一帧就推断正在飞行。
- [x] 修复浅拷贝默认对象可能共享嵌套状态的问题，使用工厂函数创建默认状态。
- [x] 飞行时间优先采用飞控时间；本地计时仅作为明确标记的降级方案。
- [x] 所有数值在 store 内保留数值类型，单位和格式化放在显示层。
- [x] 删除首页 `_mockTimer`；开发演示通过“模拟器页面/构建开关”显式启用。

**验收：** 用真实抓包回放得到确定结果；随机分块输入与错误帧不会造成崩溃或后续永久失步。

---

## 8. P0：BLE 控制链路改造

### 8.1 权限与扫描

- [ ] 按 Android API 级别处理权限：新版本使用 Nearby Devices/Bluetooth 权限，旧版本按系统要求处理定位权限。
- [ ] 运行时解释并请求必要权限，拒绝后给出可恢复入口，不循环弹窗。
- [ ] 扫描设置超时和停止按钮，页面离开时停止扫描并注销监听。
- [ ] 去重键使用 `deviceId`，处理无名称设备、缓存设备和 RSSI 更新。
- [ ] 支持按已知 Service UUID/设备标识过滤，避免用户连接无关设备。

### 8.2 连接与 GATT

- [ ] 连接后先发现服务与特征，再确认读/写/notify 属性；失败时给出具体错误。
- [ ] UUID 从设备配置或产品常量读取，不把 FFE0/FFE1 当作所有设备通用值。
- [ ] 订阅设备状态/ACK 通知并处理特征值变化。
- [ ] 根据协商 MTU 对二进制协议分包；处理写队列，禁止并发覆盖写入。
- [ ] 区分 write-with-response / write-without-response，并据此设计节流和确认。
- [ ] 将当前逐字符 `charCodeAt` 的 JSON 编码替换为协议规定的二进制编码或正确 UTF-8 编码。
- [ ] 建立连接状态机、有限指数退避重连和手动断开标记。
- [ ] 处理蓝牙被系统关闭、设备超距、GATT 错误、页面销毁等场景。

### 8.3 摇杆与飞行安全

- [ ] 明确摇杆通道与数值范围，例如 `[-1000,1000]`/PWM/角度，禁止直接发送 UI 百分比字符串。
- [ ] 增加死区、曲线、限幅、校准、模式 1/模式 2 配置。
- [ ] 触摸移动只更新最新目标值，由固定 20–50 Hz 调度器发送，频率以协议和真机测试为准。
- [ ] 抬手时按协议发送回中/保持；油门是否自动回中必须由产品安全规则决定，不能沿用普通摇杆默认行为。
- [ ] 多点触控时保证左右摇杆独立；处理系统手势取消 `touchcancel`。
- [ ] 控制链路超时后执行约定的安全值并明显告警。
- [ ] 对起飞、降落、返航、解锁、急停等高风险命令增加防误触、状态约束和 ACK。

**验收：** 持续操控 30 分钟无写队列堆积；主动关闭蓝牙、走出范围、杀后台均进入可解释的安全状态。

---

## 9. P1：统一连接编排、错误与日志

- [ ] 建立 `DeviceSessionManager`，集中编排 USB 视频、USB 遥测、BLE 控制，而不是由组件各自偷偷连接。
- [ ] 每条链路独立状态：`unavailable/idle/connecting/connected/degraded/error`。
- [ ] 定义全局错误码：平台不支持、未找到设备、授权拒绝、设备占用、格式不支持、断流、协议错误、BLE GATT 错误等。
- [ ] UI 显示“发生了什么、是否影响飞行、用户下一步做什么”，不只显示原始系统异常。
- [ ] 设备被拔出或 BLE 失联时，状态面板立即标记数据过期，控制发送停止。
- [ ] 加入诊断页面：App/插件版本、机型、系统、VID/PID、接口、视频配置、FPS、丢帧、连接错误计数。
- [ ] 日志采用环形文件和大小/天数上限；支持用户主动导出 ZIP。
- [ ] 日志对经纬度、设备序列号等敏感信息脱敏。
- [ ] 原生崩溃、ANR 和 JS 异常若接入第三方平台，先完成隐私与数据出境评估。

---

## 10. P1：Android 产品化配置

### 10.1 Manifest 与权限最小化

- [ ] 逐项审计当前 `manifest.json` 权限，任何权限都需要对应功能和测试用例。
- [ ] 仅保留 USB Host、蓝牙、必要网络、前台服务/通知（若确实后台运行）等权限。
- [ ] 若不使用手机内置相机、闪光灯、账户、电话状态、系统设置写入，删除对应权限/feature。
- [ ] 明确没有后台飞控需求时，前后台切换即停止控制与视频；若确需后台连接，设计前台服务和常驻通知。
- [ ] 所有广播接收器、Activity、Service 明确 exported 属性。
- [ ] 处理 Android 12+ Bluetooth、Android 13+ 通知、Android 14+ 前台服务相关变更（仅在实际使用时声明）。

### 10.2 应用信息与签名

- [ ] 配置正式名称、包名、图标、自适应图标、启动图、版本名称和 versionCode 策略。
- [ ] 生成并离线保管正式签名；仓库只保存签名参数模板，不提交密钥或密码。
- [ ] Debug/Test/Release 使用不同包名或签名，避免测试包覆盖正式包。
- [ ] 生成 SBOM/第三方依赖和许可证清单。
- [ ] 编写隐私政策：蓝牙附近设备、位置（如需要）、文件、网络、日志、遥测数据的用途和保留策略。

### 10.3 性能与体验

- [ ] 保持屏幕常亮仅在实际连接/飞行会话期间启用，结束后恢复。
- [ ] 记录 CPU、GPU、内存、温度、电量和 USB 供电在 720p/1080p 各模式下的表现。
- [ ] 设置性能降级策略：优先降帧率/分辨率，保证控制和遥测线程不被视频拖垮。
- [ ] 检查内存峰值、原生堆、文件描述符、线程数和 Surface 泄漏。
- [ ] 禁止 UI 主线程执行 USB 阻塞读、视频解码、协议大循环或磁盘日志压缩。

---

## 11. P1：HarmonyOS 路线

### 11.1 路线 A：支持 APK 的华为设备

- [ ] 在准确系统版本的目标机上安装同一 Android APK。
- [ ] 验证 USB Host 枚举、授权弹窗、UVC 传输、硬解码、BLE 权限与后台策略，而不是只验证能启动。
- [ ] 将华为机型测试结果加入 Android 兼容矩阵；失败按 Android 厂商兼容问题处理。

### 11.2 路线 B：HarmonyOS NEXT 原生 HAP

- [ ] 配置 DevEco Studio、证书、App ID、签名和 HAP 构建基线。
- [ ] 建立 `app-harmony` 平台实现，保持与 Android 相同的 TypeScript/UTS 公共接口。
- [ ] 调研并 POC 鸿蒙目标版本提供的 USB Host/设备授权/端点传输能力。
- [ ] 调研并 POC 外接 UVC 摄像头的格式协商、帧获取和视频解码/渲染能力。
- [ ] 通过 ArkTS 原生嵌入组件承载视频画面，验证与 Vue OSD 的同层叠加和触控事件。
- [ ] 重写 Android 专属的广播、PendingIntent、Surface、MediaCodec、AAR/SO 接入部分。
- [ ] 重新实现鸿蒙蓝牙权限、发现、GATT、生命周期和异常映射。
- [ ] 若系统公开能力无法满足目标 UVC 设备，向硬件厂商获取鸿蒙 HAR/SDK，或将“该硬件暂仅支持 Android”作为正式范围限制。
- [ ] 通过 POC 后再估算完整 HarmonyOS 版本；POC 未通过前不承诺 HAP 交付日期。

**HarmonyOS POC 验收：** HAP 在目标真机上识别同一摄像头，获得授权，连续预览 10 分钟并正确处理一次拔插。

---

## 12. P1：测试计划

### 12.1 自动化测试

- [x] 协议解析：正常帧、半帧、多帧、噪声、错误长度、错误 CRC、未知类型、超大帧。
- [x] 遥测 store：单位、边界、过期、乱序、断开、重置、飞行状态转换。
- [x] 坐标转换：旋转、镜像、黑边、裁剪、越界和最小框。
- [ ] BLE 编码/分包：MTU 边界、重试、ACK 超时、写队列覆盖策略。
- [x] 状态机：重复 open/close、打开期间拔出、授权拒绝、退后台、旧回调晚到。（**部分完成**：状态机流转与幂等已覆盖，原生回调晚到待实现后补测）
- [ ] Release 构建检查：不得包含 Mock 定时器、测试设备、开发 URL 或明文密钥。

### 12.2 Android 真机矩阵

- [ ] 每个目标 Android 大版本至少 1 台设备，主力品牌和华为目标机必测。
- [ ] 每个目标 USB 视频格式至少测试一个稳定配置和一个超带宽/不支持配置。
- [ ] 冷启动时已插设备、启动后插入、播放中拔出、快速连续插拔、授权拒绝后重试。
- [ ] 带电 Hub/不带电 OTG、低电量、省电模式、设备发热、存储空间不足。
- [ ] ADB 通过 Wi-Fi 调试 USB 外设场景，收集 logcat 和原生崩溃栈。
- [ ] 蓝牙关闭、权限拒绝、扫描超时、远离断线、设备重启、手机来电/锁屏/切后台。
- [ ] 30 分钟 MVP 稳定测试；发布前增加 2 小时 soak test。

### 12.3 性能验收指标（需与产品/硬件确认数值）

- [ ] 视频首帧时间：从授权完成到首帧显示，记录 P50/P95。
- [ ] 端到端视频延迟：使用可测量方法记录，不用肉眼估计。
- [ ] 实际采集/渲染 FPS、丢帧率、最大连续卡顿时间。
- [ ] 控制命令发送抖动、ACK 时延、失联检测时间。
- [ ] 遥测吞吐、解析错误率、UI 更新频率和数据过期阈值。
- [ ] 目标机连续运行后的 CPU、内存、温度、电量和崩溃/ANR。

### 12.4 回归验收记录

- [ ] 每次发布保存：APK/HAP、符号文件、版本信息、Git commit、测试报告、硬件固件版本和依赖清单。
- [ ] 严重问题定义：控制失效/误发、状态假在线、视频卡死无提示、拔插崩溃、资源无法恢复均为阻断发布。

---

## 13. P2：发布、运维与文档

- [ ] 编写用户手册：支持设备、OTG/供电连接顺序、USB 授权、蓝牙配对、常见错误和日志导出。
- [ ] 编写开发文档：环境、构建、调试基座、插件接口、协议、状态机、发布签名。
- [ ] 编写硬件兼容表，注明每个 VID/PID、固件、格式、最高稳定配置和已知限制。
- [ ] 建立版本升级/回滚策略；数据库或偏好配置发生变化时提供迁移。
- [ ] 若支持在线升级，只检查并下载完整受签名保护的安装包；不要让 UTS/原生核心能力依赖 Web 热更新。
- [ ] 建立问题采集模板：App 版本、手机、系统、USB 设备、固件、连接拓扑、复现步骤、诊断包。
- [ ] 发布前完成第三方许可证、隐私政策、应用商店权限说明和安全扫描。

---

## 14. 推荐目录结构（迁移后）

```text
my-uni-project/
├─ pages/
│  ├─ controller/
│  ├─ devices/
│  ├─ diagnostics/
│  └─ settings/
├─ components/
│  ├─ video-overlay/
│  ├─ joystick/
│  └─ status-panel/
├─ domain/
│  ├─ video/
│  ├─ telemetry/
│  ├─ control/
│  └─ errors/
├─ services/
│  ├─ device-session-manager.*
│  ├─ telemetry-parser.*
│  ├─ control-scheduler.*
│  └─ diagnostics.*
├─ uni_modules/
│  ├─ usb-uvc-video/
│  │  └─ utssdk/
│  │     ├─ interface.uts
│  │     ├─ app-android/
│  │     └─ app-harmony/
│  └─ device-transport/
├─ tests/
│  ├─ fixtures/
│  ├─ protocol/
│  └─ coordinate-transform/
├─ docs/
│  ├─ adr/
│  ├─ hardware-matrix.md
│  ├─ protocol.md
│  └─ release-checklist.md
└─ README.md
```

---

## 15. 建议里程碑与依赖顺序

| 里程碑 | 必须完成 | 可验收输出 | 前置依赖 |
|---|---|---|---|
| M0 真实需求冻结 | 第 2 节 | 描述符、协议、硬件矩阵 | 真实硬件/厂商资料 |
| M1 工程转正 | 第 3–4 节 | 可安装空壳 APK | 技术路线 ADR |
| M2 USB 视频 POC | 第 5 节核心项 | 真机连续预览 10 分钟 | 摄像头、OTG、目标机 |
| M3 Android MVP | 第 5–8 节 | 视频+遥测+控制闭环 APK | 真实协议、M2 |
| M4 稳定版 | 第 9–12 节 | 30 分钟/2 小时测试报告 | 多机型与测试设备 |
| M5 发布版 | 第 10、13 节 | 签名包、文档、许可证 | 签名与发布账户 |
| M6 HarmonyOS | 第 11 节 | APK 兼容报告或原生 HAP | 明确系统代际、鸿蒙 POC |

### 关键依赖关系

```text
真实硬件与协议
    ├─> Android USB/UVC POC ─> 原生视频组件 ─> 视频 UI/框选映射
    ├─> USB 遥测类型确认 ───> Transport ─────> Parser/状态面板
    └─> BLE 协议确认 ────────> GATT/分包 ────> 摇杆安全控制

三条链路完成 ─> 统一会话与错误恢复 ─> 多机型稳定测试 ─> 签名发布

Android POC 经验 + 鸿蒙公开能力/厂商 SDK ─> HarmonyOS NEXT POC ─> HAP
```

---

## 16. 第一轮实施任务（可以立即开始）

- [x] T01：建立 Git 基线，保存当前 Web 原型。
- [ ] T02：向硬件方索取 USB 描述符、VID/PID、视频格式表、遥测协议和 BLE UUID。
- [ ] T03：确定 3 台 Android 测试机与准确系统版本，准备 OTG 和带电 Hub。
- [x] T04：编写 ADR-001，确定 UniApp X / 标准 UniApp / 原生 Android 路线。（**已定**：UniApp X + UTS）
- [ ] T05：创建正式 App 模板工程并迁移一个静态页面，产出首个空壳 APK。（**进行中**：已创建 uni-app x 工程 `App.uvue`/`main.uts`/`platformConfig.json` 并迁移静态页 `pages/index/index.uvue`；空壳 APK 待 HBuilderX 云打包生成）
- [x] T06：建立 `UsbVideoAdapter` 接口和状态机，不接 UI 假数据。
- [ ] T07：用真实摄像头完成 Android 原生 UVC POC，并测枚举、授权、预览和拔插。
- [ ] T08：锁定通过 POC 的 UVC 依赖、ABI 和许可证，再封装 UTS/原生组件。
- [x] T09：删除首页随机遥测，建立可显式开关的开发 Mock。
- [ ] T10：用真实协议抓包完成遥测 parser 单元测试，再接 USB transport。（**部分完成**：parser/transport/store 与单测已就绪，真实协议抓包后替换临时帧格式即可）
- [ ] T11：验证真实 BLE 服务/特征，完成二进制编解码与固定频率控制发送。
- [x] T12：实现真机诊断页和最小测试报告模板。

---

## 17. 明确禁止的“伪完成”标准

- [ ] 不以 `npm run build` 成功或浏览器页面能打开作为 Android App 完成。
- [ ] 不以 manifest 写了 USB 权限作为 USB 视频完成。
- [ ] 不以播放网络 MP4/HLS/RTSP 地址作为 UVC 摄像头完成。
- [ ] 不以随机数更新状态面板作为遥测完成。
- [ ] 不以 BLE 扫描到设备作为控制链路完成。
- [ ] 不以华为设备能安装 APK作为 HarmonyOS NEXT 原生适配完成。
- [ ] 不在没有真实设备连续运行、插拔和异常恢复测试时声明“支持 1080P 60FPS”。

---

## 18. 实施参考（官方文档）

- Android USB Host 概览：<https://developer.android.com/develop/connectivity/usb/host>
- Android USB Host/Accessory 概览：<https://developer.android.com/develop/connectivity/usb>
- DCloud UTS 插件：<https://uniapp.dcloud.net.cn/plugin/uts-plugin.html>
- DCloud UTS Android 原生能力：<https://uniapp.dcloud.net.cn/plugin/uts-for-android.html>
- DCloud UTS 原生组件：<https://uniapp.dcloud.net.cn/plugin/uts-component.html>
- DCloud 鸿蒙原生组件嵌入：<https://uniapp.dcloud.net.cn/tutorial/harmony/native-component.html>

> 注意：具体 API、构建工具、权限和商店规则会变化。进入实施阶段时，应以锁定版本对应的官方文档和目标真机行为为准，并将实际版本写入 ADR/README。
