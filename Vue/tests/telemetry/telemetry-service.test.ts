import { describe, it, expect } from 'vitest'
import { TelemetryService, TelemetryDecoderRegistry } from '../../services/telemetry-service'
import { ReplayTransport, chunkBytes } from '../../services/telemetry-transport'
import { createTelemetryStore } from '../../domain/telemetry/store'
import {
  MSG,
  frameBytes,
  concat,
  statusPayload,
  gpsPayload,
  batteryPayload,
  readI32LE
} from '../fixtures/telemetry-fixtures'

/** 临时占位解码器（真实协议确认后替换，见 protocol.md）。 */
function buildRegistry() {
  const reg = new TelemetryDecoderRegistry()
  reg.register(MSG.STATUS, (payload) => ({
    flightStatus: payload[0] === 1 ? 'flying' : 'landed',
    imuOk: true
  }))
  reg.register(MSG.GPS, (payload) => ({
    lat: readI32LE(payload, 0) / 1e6,
    lng: readI32LE(payload, 4) / 1e6,
    satellites: payload[8]
  }))
  reg.register(MSG.BATTERY, (payload) => ({
    batteryPct: payload[0],
    voltageV: (payload[1] | (payload[2] << 8)) / 1000
  }))
  return reg
}

describe('TelemetryService（transport -> parser -> decoder -> store）', () => {
  it('回放帧流后 store 得到正确领域值', async () => {
    const transport = new ReplayTransport([])
    const store = createTelemetryStore()
    const service = new TelemetryService(transport, {
      store,
      decoders: buildRegistry()
    })
    await service.start()

    transport.push(frameBytes(MSG.STATUS, 1, statusPayload(1, 0)))
    transport.push(frameBytes(MSG.GPS, 2, gpsPayload(30.2741, 120.1551, 17)))
    transport.push(frameBytes(MSG.BATTERY, 3, batteryPayload(76, 11400)))

    expect(store.state.flightStatus).toBe('flying')
    expect(store.state.lat).toBeCloseTo(30.2741)
    expect(store.state.lng).toBeCloseTo(120.1551)
    expect(store.state.satellites).toBe(17)
    expect(store.state.batteryPct).toBe(76)
    expect(store.state.voltageV).toBeCloseTo(11.4)

    const stats = service.getStats()
    expect(stats.frames).toBe(3)
    expect(stats.decoded).toBe(3)

    await service.stop()
  })

  it('未知消息类型被计数且不影响其他帧', async () => {
    const transport = new ReplayTransport([])
    const service = new TelemetryService(transport, { decoders: buildRegistry() })
    await service.start()

    transport.push(
      concat(
        frameBytes(0x7a, 1, [1, 2, 3]),
        frameBytes(MSG.BATTERY, 2, batteryPayload(50, 11000))
      )
    )

    const stats = service.getStats()
    expect(stats.frames).toBe(2)
    expect(stats.decoded).toBe(1)
    expect(stats.unknownType).toBe(1)
    expect(service.store.state.batteryPct).toBe(50)

    await service.stop()
  })

  it('随机分块回放不丢失帧', async () => {
    const bytes = concat(
      frameBytes(MSG.STATUS, 1, statusPayload(1, 0)),
      frameBytes(MSG.GPS, 2, gpsPayload(31, 121, 12)),
      frameBytes(MSG.BATTERY, 3, batteryPayload(88, 11200))
    )
    const transport = new ReplayTransport(chunkBytes(bytes, [1, 4, 2, 9, 3]))
    const service = new TelemetryService(transport, { decoders: buildRegistry() })
    await service.start()
    transport.replayAll()

    expect(service.getStats().decoded).toBe(3)
    await service.stop()
  })

  it('stop 后链路标记为断开', async () => {
    const transport = new ReplayTransport([])
    const service = new TelemetryService(transport, { decoders: buildRegistry() })
    await service.start()
    transport.push(frameBytes(MSG.STATUS, 1, statusPayload(1, 0)))
    await service.stop()
    expect(service.store.state.link).toBe('idle')
    expect(service.store.state.flightStatus).toBe('disconnected')
  })
})
