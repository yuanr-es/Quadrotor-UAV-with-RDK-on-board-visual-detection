/**
 * 测试夹具：按临时占位帧格式构造字节流。
 *
 * 真实协议确认后，替换此处的消息类型与载荷编码。
 */

import { encodeFrame, ParsedFrame } from '../../services/telemetry-parser'

/** 临时占位消息类型（待 protocol.md 确认后替换）。 */
export const MSG = {
  STATUS: 0x01,
  GPS: 0x02,
  BATTERY: 0x04
} as const

export function frameBytes(type: number, seq: number, payload: number[]): Uint8Array {
  return encodeFrame({ type, seq, payload: Uint8Array.from(payload) })
}

export function statusPayload(flightStatus: number, flightMode: number): number[] {
  return [flightStatus, flightMode]
}

export function gpsPayload(lat: number, lng: number, sats: number): number[] {
  const b: number[] = []
  pushI32LE(b, Math.round(lat * 1e6))
  pushI32LE(b, Math.round(lng * 1e6))
  b.push(sats)
  return b
}

export function batteryPayload(pct: number, voltageMv: number): number[] {
  const b: number[] = [pct]
  b.push(voltageMv & 0xff, (voltageMv >> 8) & 0xff)
  return b
}

export function concat(...arrays: Uint8Array[]): Uint8Array {
  const total = arrays.reduce((n, a) => n + a.length, 0)
  const out = new Uint8Array(total)
  let o = 0
  for (const a of arrays) {
    out.set(a, o)
    o += a.length
  }
  return out
}

function pushI32LE(arr: number[], v: number): void {
  const u = v >>> 0
  arr.push(u & 0xff, (u >>> 8) & 0xff, (u >>> 16) & 0xff, (u >>> 24) & 0xff)
}

export function readI32LE(b: Uint8Array, off: number): number {
  return (b[off] | (b[off + 1] << 8) | (b[off + 2] << 16) | (b[off + 3] << 24)) >>> 0
}

export type { ParsedFrame }
