/**
 * UsbVideoAdapter 统一接口（TypeScript 镜像）
 *
 * 与 `uni_modules/usb-uvc-video/utssdk/interface.uts` 保持一致。
 * Android 由 UTS/原生实现，HarmonyOS 由 ArkTS 实现，开发期由 Mock 实现。
 * 参见 TODOList.md §3.3、§5.3。
 */

import type {
  AdapterEventName,
  AdapterEventPayloadMap,
  UsbDevice,
  VideoCapability,
  VideoConfig,
  VideoStats
} from './types'

export type AdapterListener<E extends AdapterEventName> = (
  payload: AdapterEventPayloadMap[E]
) => void

export interface UsbVideoAdapter {
  /** 初始化平台 USB/媒体子系统，可重复调用（幂等） */
  initialize(): Promise<void>

  /** 枚举当前已连接的候选设备 */
  listDevices(): Promise<UsbDevice[]>

  /** 请求某个设备的 USB 访问授权 */
  requestPermission(deviceId: string): Promise<boolean>

  /** 查询设备真实能力（格式/分辨率/帧率） */
  getCapability(deviceId: string): Promise<VideoCapability>

  /** 打开设备，分配会话，返回 sessionId */
  open(deviceId: string, config: VideoConfig): Promise<number>

  /** 开始预览 */
  startPreview(sessionId: number): Promise<void>

  /** 停止预览（保留会话） */
  stopPreview(sessionId: number): Promise<void>

  /** 关闭并释放会话资源（幂等） */
  close(sessionId: number): Promise<void>

  /** 读取最近一次统计 */
  getStats(sessionId: number): VideoStats | null

  /** 订阅事件，返回取消订阅函数 */
  on<E extends AdapterEventName>(
    event: E,
    listener: AdapterListener<E>
  ): () => void

  /** 释放平台资源（退出前调用） */
  release(): Promise<void>
}
