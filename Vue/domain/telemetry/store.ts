/**
 * 遥测 store
 *
 * 只接收已经过校验的 `TelemetryFrame`；连接状态与飞行状态分离；
 * 数值保持数值类型，格式化交给显示层（useTelemetryView）。
 * 默认状态用工厂函数创建，避免浅拷贝共享嵌套对象。
 * 参见 TODOList.md §7.3、§9。
 */

import { computed, reactive, readonly } from 'vue'
import type { FlightStatus, LinkStatus, TelemetryFrame } from './types'

export interface TelemetryState {
  link: LinkStatus
  flightStatus: FlightStatus
  /** 收到过至少一帧后为 true，仅表示链路有数据，不推断飞行 */
  hasData: boolean
  /** 超过 staleThresholdMs 未更新 */
  stale: boolean
  lastFrameAt: number
  name: string
  signalStrength: number | null
  satellites: number | null
  lat: number | null
  lng: number | null
  altitudeM: number | null
  hSpeedMps: number | null
  vSpeedMps: number | null
  distanceM: number | null
  flightMode: string
  imuOk: boolean
  compassOk: boolean
  batteryPct: number | null
  voltageV: number | null
  remainingS: number | null
  fcTimeMs: number | null
  /** 本地飞行计时（降级方案，默认 0） */
  flightTimeMs: number
}

export const DEFAULT_STALE_THRESHOLD_MS = 2000

function createDefaultState(): TelemetryState {
  return {
    link: 'idle',
    flightStatus: 'disconnected',
    hasData: false,
    stale: false,
    lastFrameAt: 0,
    name: '未知设备',
    signalStrength: null,
    satellites: null,
    lat: null,
    lng: null,
    altitudeM: null,
    hSpeedMps: null,
    vSpeedMps: null,
    distanceM: null,
    flightMode: '--',
    imuOk: false,
    compassOk: false,
    batteryPct: null,
    voltageV: null,
    remainingS: null,
    fcTimeMs: null,
    flightTimeMs: 0
  }
}

export function createTelemetryStore(staleThresholdMs = DEFAULT_STALE_THRESHOLD_MS) {
  const state = reactive<TelemetryState>(createDefaultState())

  let flightTimer: ReturnType<typeof setInterval> | null = null
  let flightStartAt = 0

  function startFlightTimer(now: number) {
    if (flightTimer) return
    flightStartAt = now - state.flightTimeMs
    flightTimer = setInterval(() => {
      state.flightTimeMs = Date.now() - flightStartAt
    }, 1000)
  }

  function stopFlightTimer() {
    if (flightTimer) {
      clearInterval(flightTimer)
      flightTimer = null
    }
  }

  return {
    state,

    /** 应用一帧已校验遥测。now 可注入以便测试。 */
    update(frame: TelemetryFrame, now: number = Date.now()) {
      state.hasData = true
      state.stale = false
      state.lastFrameAt = now
      if (state.link !== 'connected') state.link = 'connected'

      if (frame.name !== undefined) state.name = frame.name
      if (frame.signalStrength !== undefined) state.signalStrength = frame.signalStrength
      if (frame.satellites !== undefined) state.satellites = frame.satellites
      if (frame.lat !== undefined) state.lat = frame.lat
      if (frame.lng !== undefined) state.lng = frame.lng
      if (frame.altitudeM !== undefined) state.altitudeM = frame.altitudeM
      if (frame.hSpeedMps !== undefined) state.hSpeedMps = frame.hSpeedMps
      if (frame.vSpeedMps !== undefined) state.vSpeedMps = frame.vSpeedMps
      if (frame.distanceM !== undefined) state.distanceM = frame.distanceM
      if (frame.flightMode !== undefined) state.flightMode = frame.flightMode
      if (frame.imuOk !== undefined) state.imuOk = frame.imuOk
      if (frame.compassOk !== undefined) state.compassOk = frame.compassOk
      if (frame.batteryPct !== undefined) state.batteryPct = frame.batteryPct
      if (frame.voltageV !== undefined) state.voltageV = frame.voltageV
      if (frame.remainingS !== undefined) state.remainingS = frame.remainingS
      if (frame.fcTimeMs !== undefined) state.fcTimeMs = frame.fcTimeMs

      if (frame.flightStatus !== undefined) {
        state.flightStatus = frame.flightStatus
        if (frame.flightStatus === 'flying') startFlightTimer(now)
        else stopFlightTimer()
      }
    },

    setLink(status: LinkStatus) {
      state.link = status
    },

    markDisconnected() {
      state.link = 'idle'
      state.flightStatus = 'disconnected'
      state.stale = false
      stopFlightTimer()
    },

    /** 检测数据过期。返回是否处于过期状态。 */
    checkStale(now: number = Date.now(), thresholdMs: number = staleThresholdMs): boolean {
      if (!state.hasData || state.link === 'idle') return false
      const expired = now - state.lastFrameAt > thresholdMs
      if (expired) {
        state.stale = true
        state.link = 'degraded'
      }
      return expired
    },

    reset() {
      stopFlightTimer()
      Object.assign(state, createDefaultState())
    }
  }
}

export type TelemetryStore = ReturnType<typeof createTelemetryStore>

// ==================== 应用单例 ====================

export const telemetryStore = createTelemetryStore()

export function useTelemetry() {
  return readonly(telemetryStore.state)
}

/** 显示层格式化：把数值域对象转换为带单位的字符串。 */
export function formatTelemetry(state: Readonly<TelemetryState>) {
  const gps = {
    lat: state.lat === null ? '--' : `${state.lat.toFixed(4)}° ${state.lat >= 0 ? 'N' : 'S'}`,
    lng: state.lng === null ? '--' : `${state.lng.toFixed(4)}° ${state.lng >= 0 ? 'E' : 'W'}`
  }

  const effectiveStatus: FlightStatus =
    state.link === 'connected' && !state.stale ? state.flightStatus : 'disconnected'

  return {
    name: state.name,
    connected: state.link === 'connected' && !state.stale,
    stale: state.stale,
    status: effectiveStatus,
    signalStrength: state.signalStrength === null ? '--' : state.signalStrength,
    satellites: state.satellites === null ? '--' : state.satellites,
    gps,
    altitude: state.altitudeM === null ? '--' : state.altitudeM.toFixed(1),
    hSpeed: state.hSpeedMps === null ? '--' : state.hSpeedMps.toFixed(1),
    vSpeed: state.vSpeedMps === null ? '--' : state.vSpeedMps.toFixed(1),
    distance: state.distanceM === null ? '--' : state.distanceM.toFixed(1),
    flightMode: state.flightMode,
    imuOk: state.imuOk,
    compassOk: state.compassOk,
    battery: state.batteryPct === null ? 0 : Math.round(state.batteryPct),
    voltage: state.voltageV === null ? '--' : state.voltageV.toFixed(1),
    remainingTime: state.remainingS === null ? '--' : Math.round(state.remainingS / 60),
    flightTime: formatDuration(state.flightTimeMs)
  }
}

export function useTelemetryView() {
  return computed(() => formatTelemetry(telemetryStore.state))
}

export function formatDuration(ms: number): string {
  const total = Math.max(0, Math.floor(ms / 1000))
  const h = String(Math.floor(total / 3600)).padStart(2, '0')
  const m = String(Math.floor((total % 3600) / 60)).padStart(2, '0')
  const s = String(total % 60).padStart(2, '0')
  return `${h}:${m}:${s}`
}
