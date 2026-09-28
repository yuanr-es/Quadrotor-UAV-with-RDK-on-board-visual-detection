/**
 * 开发用遥测 Mock —— **必须显式启用**
 *
 * 首页不得再自行产生随机遥测（TODOList.md §0.1、§7.3、§17）。
 * 启用方式（满足其一）：
 *   1. 环境变量 VITE_ENABLE_MOCK_TELEMETRY=true
 *   2. 浏览器调试时地址带 ?mock=1
 * Release 构建必须不设置该变量。
 *
 * 注意：这是「领域层 Mock」，直接产生 TelemetryFrame；真实链路请走
 * services/telemetry-service.ts 的 transport -> parser -> decoder -> store。
 */

import type { TelemetryFrame } from '../domain/telemetry/types'
import type { createTelemetryStore } from '../domain/telemetry/store'

export function isMockTelemetryEnabled(): boolean {
  const env = (import.meta as any)?.env
  if (env?.VITE_ENABLE_MOCK_TELEMETRY === 'true') return true
  if (typeof window !== 'undefined' && window.location) {
    try {
      return new URLSearchParams(window.location.search).get('mock') === '1'
    } catch {
      return false
    }
  }
  return false
}

/** 启动 Mock 遥测，返回停止函数。 */
export function startMockTelemetry(
  store: ReturnType<typeof createTelemetryStore>,
  intervalMs = 1000
): () => void {
  const timer = setInterval(() => {
    const frame: TelemetryFrame = {
      flightStatus: 'flying',
      signalStrength: Math.floor(Math.random() * 5) + 10,
      satellites: Math.floor(Math.random() * 4) + 16,
      lat: 30.2741 + Math.random() * 0.001,
      lng: 120.1551 + Math.random() * 0.001,
      altitudeM: 128 + Math.random() * 5,
      hSpeedMps: 8 + Math.random() * 2,
      vSpeedMps: Math.random() * 2 - 0.5,
      distanceM: 356 + Math.random() * 10,
      flightMode: 'GPS 模式',
      imuOk: true,
      compassOk: true,
      batteryPct: Math.max(70, Math.floor(76 - Math.random() * 2)),
      voltageV: 11.4,
      remainingS: Math.floor(16 + Math.random() * 4) * 60,
      fcTimeMs: Date.now()
    }
    store.update(frame)
  }, intervalMs)

  return () => clearInterval(timer)
}
