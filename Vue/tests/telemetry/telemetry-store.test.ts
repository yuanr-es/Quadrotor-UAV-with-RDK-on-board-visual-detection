import { describe, it, expect } from 'vitest'
import {
  createTelemetryStore,
  formatTelemetry,
  formatDuration
} from '../../domain/telemetry/store'

describe('telemetry store', () => {
  it('默认状态为未连接且不推断飞行', () => {
    const store = createTelemetryStore()
    expect(store.state.link).toBe('idle')
    expect(store.state.flightStatus).toBe('disconnected')
    expect(store.state.hasData).toBe(false)
  })

  it('收到数据只更新连接与字段，不推断飞行状态', () => {
    const store = createTelemetryStore()
    store.update({ altitudeM: 120, batteryPct: 80 }, 1000)
    expect(store.state.link).toBe('connected')
    expect(store.state.hasData).toBe(true)
    // 未上报 flightStatus，不能变成 flying
    expect(store.state.flightStatus).toBe('disconnected')
    expect(store.state.altitudeM).toBe(120)
    expect(typeof store.state.altitudeM).toBe('number')
  })

  it('数值保持数值类型（不格式化为字符串）', () => {
    const store = createTelemetryStore()
    store.update({ lat: 30.2741, voltageV: 11.4 })
    expect(store.state.lat).toBeCloseTo(30.2741)
    expect(store.state.voltageV).toBe(11.4)
  })

  it('超过阈值未更新进入 degraded / stale', () => {
    const store = createTelemetryStore(2000)
    store.update({ batteryPct: 90 }, 1000)
    expect(store.checkStale(2500, 2000)).toBe(false)
    expect(store.checkStale(3500, 2000)).toBe(true)
    expect(store.state.stale).toBe(true)
    expect(store.state.link).toBe('degraded')
  })

  it('markDisconnected 重置飞行状态', () => {
    const store = createTelemetryStore()
    store.update({ flightStatus: 'flying' }, 1000)
    expect(store.state.flightStatus).toBe('flying')
    store.markDisconnected()
    expect(store.state.flightStatus).toBe('disconnected')
    expect(store.state.link).toBe('idle')
  })

  it('两个 store 实例状态互相独立（工厂默认值）', () => {
    const a = createTelemetryStore()
    const b = createTelemetryStore()
    a.update({ altitudeM: 50 })
    expect(b.state.altitudeM).toBeNull()
  })

  it('reset 恢复默认', () => {
    const store = createTelemetryStore()
    store.update({ altitudeM: 50, batteryPct: 10 })
    store.reset()
    expect(store.state.altitudeM).toBeNull()
    expect(store.state.batteryPct).toBeNull()
    expect(store.state.link).toBe('idle')
  })
})

describe('formatTelemetry（显示层）', () => {
  it('无数据时显示占位符', () => {
    const store = createTelemetryStore()
    const v = formatTelemetry(store.state)
    expect(v.altitude).toBe('--')
    expect(v.gps.lat).toBe('--')
    expect(v.status).toBe('disconnected')
    expect(v.connected).toBe(false)
  })

  it('有数据时格式化单位与方向', () => {
    const store = createTelemetryStore()
    store.update({
      lat: 30.2741,
      lng: 120.1551,
      altitudeM: 128.46,
      voltageV: 11.4,
      batteryPct: 76.6,
      flightStatus: 'flying'
    })
    const v = formatTelemetry(store.state)
    expect(v.gps.lat).toBe('30.2741° N')
    expect(v.gps.lng).toBe('120.1551° E')
    expect(v.altitude).toBe('128.5')
    expect(v.voltage).toBe('11.4')
    expect(v.battery).toBe(77)
    expect(v.status).toBe('flying')
  })

  it('数据过期时不显示飞行中', () => {
    const store = createTelemetryStore(1000)
    store.update({ flightStatus: 'flying' }, 1000)
    store.checkStale(3000, 1000)
    const v = formatTelemetry(store.state)
    expect(v.status).toBe('disconnected')
    expect(v.connected).toBe(false)
  })

  it('南纬西经方向正确', () => {
    const store = createTelemetryStore()
    store.update({ lat: -12.34, lng: -56.78 })
    const v = formatTelemetry(store.state)
    expect(v.gps.lat).toBe('-12.3400° S')
    expect(v.gps.lng).toBe('-56.7800° W')
  })
})

describe('formatDuration', () => {
  it('格式化 HH:MM:SS', () => {
    expect(formatDuration(0)).toBe('00:00:00')
    expect(formatDuration(65_000)).toBe('00:01:05')
    expect(formatDuration(3_661_000)).toBe('01:01:01')
  })
})
