/**
 * 遥测领域模型
 *
 * store 只接收这里定义、且已经过校验的领域对象；单位与格式化属于显示层。
 * 参见 TODOList.md §7.2、§7.3。
 */

/** 飞行状态（与连接状态分离，§7.3） */
export type FlightStatus = 'flying' | 'landed' | 'rth' | 'disconnected'

/** 连接状态（与飞行状态分离） */
export type LinkStatus =
  | 'unavailable'
  | 'idle'
  | 'connecting'
  | 'connected'
  | 'degraded'
  | 'error'

/**
 * 一帧已校验的遥测领域对象（数值类型，单位为国际单位）。
 * 字段全部可选：只有设备真实上报的字段才更新。
 */
export interface TelemetryFrame {
  name?: string
  flightStatus?: FlightStatus
  /** 链路信号强度，原始单位由协议定义 */
  signalStrength?: number
  satellites?: number
  /** 纬度，度 */
  lat?: number
  /** 经度，度 */
  lng?: number
  /** 海拔，米 */
  altitudeM?: number
  /** 水平速度，米/秒 */
  hSpeedMps?: number
  /** 垂直速度，米/秒 */
  vSpeedMps?: number
  /** 距起飞点距离，米 */
  distanceM?: number
  flightMode?: string
  imuOk?: boolean
  compassOk?: boolean
  /** 电量百分比 0-100 */
  batteryPct?: number
  /** 电压，伏 */
  voltageV?: number
  /** 剩余续航，秒 */
  remainingS?: number
  /** 飞控时间戳 ms（优先使用，§7.3） */
  fcTimeMs?: number
}
