import { describe, it, expect } from 'vitest'
import {
  TelemetryFrameParser,
  encodeFrame,
  crc16ccitt
} from '../../services/telemetry-parser'
import {
  MSG,
  frameBytes,
  concat,
  statusPayload,
  gpsPayload
} from '../fixtures/telemetry-fixtures'

describe('TelemetryFrameParser', () => {
  it('解析单帧', () => {
    const parser = new TelemetryFrameParser()
    const bytes = frameBytes(MSG.STATUS, 7, statusPayload(1, 2))
    const frames = parser.feed(bytes)
    expect(frames).toHaveLength(1)
    expect(frames[0].type).toBe(MSG.STATUS)
    expect(frames[0].seq).toBe(7)
    expect(Array.from(frames[0].payload)).toEqual([1, 2])
    expect(parser.stats.frames).toBe(1)
  })

  it('半帧到达后再补全（拆包）', () => {
    const parser = new TelemetryFrameParser()
    const bytes = frameBytes(MSG.GPS, 1, gpsPayload(30.2741, 120.1551, 18))
    const half = Math.floor(bytes.length / 2)
    expect(parser.feed(bytes.slice(0, half))).toHaveLength(0)
    const rest = parser.feed(bytes.slice(half))
    expect(rest).toHaveLength(1)
    expect(rest[0].type).toBe(MSG.GPS)
  })

  it('一个数据块含多帧（粘包）', () => {
    const parser = new TelemetryFrameParser()
    const chunk = concat(
      frameBytes(MSG.STATUS, 1, statusPayload(1, 0)),
      frameBytes(MSG.GPS, 2, gpsPayload(30, 120, 10)),
      frameBytes(MSG.BATTERY, 3, [88])
    )
    const frames = parser.feed(chunk)
    expect(frames.map((f) => f.type)).toEqual([MSG.STATUS, MSG.GPS, MSG.BATTERY])
  })

  it('忽略帧外噪声并可恢复', () => {
    const parser = new TelemetryFrameParser()
    const chunk = concat(
      Uint8Array.from([0x00, 0x11, 0x22]),
      frameBytes(MSG.STATUS, 5, statusPayload(0, 1))
    )
    const frames = parser.feed(chunk)
    expect(frames).toHaveLength(1)
    expect(parser.stats.droppedBytes).toBe(3)
  })

  it('CRC 错误被计数且不产出帧，后续帧仍可恢复', () => {
    const parser = new TelemetryFrameParser()
    const good = frameBytes(MSG.STATUS, 1, statusPayload(1, 1))
    const bad = good.slice()
    // 破坏一个载荷字节（索引 5 = payload[0]，不影响长度字段）
    bad[5] = bad[5] ^ 0xff
    const frames = parser.feed(concat(bad, good))
    expect(frames).toHaveLength(1)
    expect(parser.stats.crcErrors).toBe(1)
  })

  it('长度非法被计数', () => {
    const parser = new TelemetryFrameParser({ maxPayload: 4 })
    const bytes = frameBytes(MSG.STATUS, 1, [1, 2, 3, 4, 5, 6])
    const frames = parser.feed(bytes)
    expect(frames).toHaveLength(0)
    expect(parser.stats.lengthErrors).toBe(1)
  })

  it('转义字节（0x7E/0x7D）正确还原', () => {
    const parser = new TelemetryFrameParser()
    const payload = [0x7e, 0x7d, 0x20, 0x5e]
    const frames = parser.feed(frameBytes(MSG.STATUS, 9, payload))
    expect(frames).toHaveLength(1)
    expect(Array.from(frames[0].payload)).toEqual(payload)
  })

  it('随机分块输入不崩溃且不永久失步', () => {
    const parser = new TelemetryFrameParser()
    const bytes = concat(
      frameBytes(MSG.STATUS, 1, statusPayload(1, 0)),
      frameBytes(MSG.GPS, 2, gpsPayload(30, 120, 9)),
      frameBytes(MSG.BATTERY, 3, [50])
    )
    const sizes = [1, 3, 2, 7, 5, 11, 4]
    const collected: number[] = []
    let i = 0
    let k = 0
    while (i < bytes.length) {
      const size = sizes[k % sizes.length]
      const out = parser.feed(bytes.slice(i, i + size))
      out.forEach((f) => collected.push(f.type))
      i += size
      k++
    }
    expect(collected).toEqual([MSG.STATUS, MSG.GPS, MSG.BATTERY])
  })

  it('encodeFrame / crc16 自洽', () => {
    const data = Uint8Array.from([1, 2, 3, 4])
    expect(crc16ccitt(data)).toBe(crc16ccitt(data))
    const bytes = encodeFrame({ type: 1, seq: 2, payload: data })
    expect(bytes[0]).toBe(0x7e)
    expect(bytes[bytes.length - 1]).toBe(0x7e)
  })
})
