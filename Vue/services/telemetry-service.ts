/**
 * 遥测应用层编排：transport -> parser -> decoder -> store
 *
 * 解码器按消息类型注册；真实消息类型编码在 protocol.md 确认后注册，
 * 不允许未约定就臆造协议（参见 TODOList.md §7.2）。
 */

import { TelemetryFrameParser, ParsedFrame } from './telemetry-parser'
import type { TelemetryTransport } from './telemetry-transport'
import type { TelemetryFrame } from '../domain/telemetry/types'
import { createTelemetryStore, DEFAULT_STALE_THRESHOLD_MS } from '../domain/telemetry/store'

export type MessageDecoder = (
  payload: Uint8Array,
  frame: ParsedFrame
) => Partial<TelemetryFrame>

export class TelemetryDecoderRegistry {
  private decoders = new Map<number, MessageDecoder>()

  register(type: number, decoder: MessageDecoder): this {
    this.decoders.set(type, decoder)
    return this
  }

  has(type: number): boolean {
    return this.decoders.has(type)
  }

  decode(frame: ParsedFrame): TelemetryFrame | null {
    const decoder = this.decoders.get(frame.type)
    if (!decoder) return null
    const partial = decoder(frame.payload, frame)
    return partial as TelemetryFrame
  }
}

export interface TelemetryServiceStats {
  frames: number
  decoded: number
  unknownType: number
  crcErrors: number
  lengthErrors: number
  droppedBytes: number
}

export interface TelemetryServiceOptions {
  parser?: TelemetryFrameParser
  store?: ReturnType<typeof createTelemetryStore>
  decoders?: TelemetryDecoderRegistry
  staleThresholdMs?: number
  staleCheckIntervalMs?: number
}

export class TelemetryService {
  readonly transport: TelemetryTransport
  readonly parser: TelemetryFrameParser
  readonly store: ReturnType<typeof createTelemetryStore>
  readonly decoders: TelemetryDecoderRegistry

  private offData: (() => void) | null = null
  private offError: (() => void) | null = null
  private staleTimer: ReturnType<typeof setInterval> | null = null
  private started = false
  private staleCheckIntervalMs: number
  private stats = { frames: 0, decoded: 0, unknownType: 0 }
  private lastError: Error | null = null

  constructor(transport: TelemetryTransport, opts: TelemetryServiceOptions = {}) {
    this.transport = transport
    this.parser = opts.parser ?? new TelemetryFrameParser()
    this.store = opts.store ?? createTelemetryStore(opts.staleThresholdMs)
    this.decoders = opts.decoders ?? new TelemetryDecoderRegistry()
    this.staleCheckIntervalMs = opts.staleCheckIntervalMs ?? 1000
  }

  async start(): Promise<void> {
    if (this.started) return
    this.started = true
    await this.transport.open()

    this.offData = this.transport.onData((chunk) => this.handleChunk(chunk))
    this.offError = this.transport.onError((err) => {
      this.lastError = err
      this.store.setLink('error')
    })

    const interval = this.staleCheckIntervalMs ?? 1000
    this.staleTimer = setInterval(() => this.store.checkStale(), interval)
  }

  async stop(): Promise<void> {
    if (!this.started) return
    this.started = false
    if (this.offData) this.offData()
    if (this.offError) this.offError()
    if (this.staleTimer) {
      clearInterval(this.staleTimer)
      this.staleTimer = null
    }
    this.offData = null
    this.offError = null
    await this.transport.close()
    this.store.markDisconnected()
  }

  getStats(): TelemetryServiceStats {
    const p = this.parser.stats
    return {
      frames: this.stats.frames,
      decoded: this.stats.decoded,
      unknownType: this.stats.unknownType,
      crcErrors: p.crcErrors,
      lengthErrors: p.lengthErrors,
      droppedBytes: p.droppedBytes
    }
  }

  private handleChunk(chunk: Uint8Array): void {
    const frames = this.parser.feed(chunk)
    for (const frame of frames) {
      this.stats.frames++
      const decoded = this.decoders.decode(frame)
      if (!decoded) {
        this.stats.unknownType++
        continue
      }
      this.stats.decoded++
      this.store.update(decoded)
    }
  }
}
