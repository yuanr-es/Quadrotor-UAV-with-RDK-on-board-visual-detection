/**
 * 视频领域模型
 *
 * 与平台无关的类型定义，被 UTS 适配器接口、状态机与 UI 共同引用。
 * 参见 TODOList.md §3.3、§5.5、§6。
 */

/** USB 视频像素/编码格式 */
export type VideoFormat =
  | 'MJPEG'
  | 'YUY2'
  | 'NV12'
  | 'H264'
  | 'H265'
  | 'UNKNOWN'

/** USB 传输类型 */
export type UsbTransferType = 'isochronous' | 'bulk' | 'unknown'

/** USB 设备描述（不含平台私有句柄） */
export interface UsbDevice {
  /** 平台内唯一设备标识 */
  deviceId: string
  vendorId: number
  productId: number
  /** 产品展示名，可能为空 */
  name: string
  /** 序列号，可能为空且不可作为唯一键 */
  serial: string
  /** 接口类编号列表，用于判断是否疑似 UVC */
  interfaceClasses: number[]
}

/** 媒体格式一项（来自设备真实能力枚举） */
export interface VideoFormatCapability {
  format: VideoFormat
  widths: number[]
  heights: number[]
  /** 该格式支持的帧率 */
  fps: number[]
}

/** 设备完整能力（UI 只能展示这里真实存在的能力，禁止硬编码） */
export interface VideoCapability {
  transferType: UsbTransferType
  formats: VideoFormatCapability[]
}

/** 一次会话要使用的视频配置 */
export interface VideoConfig {
  format: VideoFormat
  width: number
  height: number
  fps: number
  /** 顺时针旋转角度：0/90/180/270 */
  rotation: 0 | 90 | 180 | 270
  mirror: boolean
}

/** 运行时视频统计（由插件事件更新，不写死） */
export interface VideoStats {
  /** 实际采集帧率 */
  captureFps: number
  /** 实际渲染帧率 */
  renderFps: number
  /** 累计丢帧 */
  droppedFrames: number
  /** 平均解码耗时 ms */
  decodeMs: number
  /** 采集队列深度 */
  queueDepth: number
  /** 最近一帧时间戳（ms） */
  lastFrameAt: number
  /** USB 错误计数 */
  usbErrors: number
}

/** 会话状态机状态（§5.5） */
export type VideoSessionState =
  | 'idle'
  | 'detected'
  | 'permissionPending'
  | 'opening'
  | 'streaming'
  | 'stopping'
  | 'closed'
  | 'error'

/** 统一错误码（§9） */
export type AdapterErrorCode =
  | 'PLATFORM_UNSUPPORTED'
  | 'DEVICE_NOT_FOUND'
  | 'PERMISSION_DENIED'
  | 'DEVICE_BUSY'
  | 'FORMAT_UNSUPPORTED'
  | 'STREAM_LOST'
  | 'PROTOCOL_ERROR'
  | 'UNKNOWN'

export interface AdapterError {
  code: AdapterErrorCode
  message: string
  /** 出错时所属会话，防止旧会话回调污染新连接 */
  sessionId: number
}

/** 适配器对外事件名（§3.3） */
export type AdapterEventName =
  | 'attached'
  | 'detached'
  | 'permissionResult'
  | 'opened'
  | 'streamStarted'
  | 'stats'
  | 'error'
  | 'closed'

/** 带 sessionId 的事件负载基类 */
export interface SessionScoped {
  sessionId: number
}

export interface AttachedEvent extends SessionScoped { device: UsbDevice }
export interface DetachedEvent extends SessionScoped { deviceId: string }
export interface PermissionResultEvent extends SessionScoped { granted: boolean }
export interface OpenedEvent extends SessionScoped { config: VideoConfig }
export interface StreamStartedEvent extends SessionScoped { config: VideoConfig }
export interface StatsEvent extends SessionScoped { stats: VideoStats }
export interface ErrorEvent extends SessionScoped { error: AdapterError }
export interface ClosedEvent extends SessionScoped { reason: string }

export interface AdapterEventPayloadMap {
  attached: AttachedEvent
  detached: DetachedEvent
  permissionResult: PermissionResultEvent
  opened: OpenedEvent
  streamStarted: StreamStartedEvent
  stats: StatsEvent
  error: ErrorEvent
  closed: ClosedEvent
}
