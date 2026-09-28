/**
 * 遥测二进制分帧与校验（平台无关）
 *
 * 重要：真实协议的帧头、长度字段、字节序、CRC 与转义规则 **尚未提供**
 * （见 docs/protocol.md）。本文件实现的是可配置的分帧/校验 **机制**，
 * 其中的默认帧格式为「临时占位」，必须在拿到真实协议后替换常量与测试夹具。
 *
 * 临时占位帧格式（HDLC 式定界，便于处理任意二进制载荷与粘包/拆包）：
 *
 *   SOF(0x7E)  定界
 *   转义(0x7D)：0x7E -> 0x7D 0x5E；0x7D -> 0x7D 0x5D   （即 0x7D, byte^0x20）
 *   帧体（未转义）：type(1) | seq(1) | len(2, LE) | payload(len) | crc16(2, LE)
 *   CRC16 覆盖 type|seq|len|payload
 *
 * 参见 TODOList.md §7.1、§7.2。
 */

export interface FrameCodec {
  /** 帧定界符 */
  sof: number
  /** 转义符 */
  escape: number
  /** 转义异或掩码 */
  escapeXor: number
  /** 帧体固定头长度：type(1)+seq(1)+len(2) */
  headerSize: number
  /** 单帧最大载荷字节数，超出记为 lengthError */
  maxPayload: number
  /** CRC16-CCITT */
  crc16(data: Uint8Array): number
}

export const DEFAULT_FRAME_CODEC: FrameCodec = {
  sof: 0x7e,
  escape: 0x7d,
  escapeXor: 0x20,
  headerSize: 4,
  maxPayload: 1024,
  crc16: crc16ccitt
}

export interface ParsedFrame {
  type: number
  seq: number
  payload: Uint8Array
}

export interface ParserStats {
  frames: number
  crcErrors: number
  lengthErrors: number
  /** 帧外噪声或非法丢弃的字节数 */
  droppedBytes: number
}

export class TelemetryFrameParser {
  private codec: FrameCodec
  private inFrame = false
  private escapePending = false
  private buf: number[] = []
  private _stats: ParserStats = {
    frames: 0,
    crcErrors: 0,
    lengthErrors: 0,
    droppedBytes: 0
  }

  constructor(codec: Partial<FrameCodec> = {}) {
    this.codec = { ...DEFAULT_FRAME_CODEC, ...codec }
  }

  get stats(): Readonly<ParserStats> {
    return this._stats
  }

  /** 处理任意分块的字节流，返回本次能解析出的完整帧（可能为空）。 */
  feed(chunk: Uint8Array): ParsedFrame[] {
    const out: ParsedFrame[] = []
    for (let i = 0; i < chunk.length; i++) {
      const b = chunk[i]

      if (b === this.codec.sof) {
        if (this.inFrame) {
          const frame = this.tryDecode()
          if (frame) out.push(frame)
        }
        this.inFrame = true
        this.escapePending = false
        this.buf = []
        continue
      }

      if (!this.inFrame) {
        this._stats.droppedBytes++
        continue
      }

      if (b === this.codec.escape) {
        this.escapePending = true
        continue
      }

      this.buf.push(this.escapePending ? b ^ this.codec.escapeXor : b)
      this.escapePending = false
    }
    return out
  }

  reset(): void {
    this.inFrame = false
    this.escapePending = false
    this.buf = []
    this._stats = { frames: 0, crcErrors: 0, lengthErrors: 0, droppedBytes: 0 }
  }

  private tryDecode(): ParsedFrame | null {
    const { headerSize, maxPayload } = this.codec
    const buf = this.buf

    if (buf.length < headerSize + 2) {
      this._stats.lengthErrors++
      return null
    }

    const len = buf[2] | (buf[3] << 8)
    if (len > maxPayload || buf.length < headerSize + len + 2) {
      this._stats.lengthErrors++
      return null
    }

    const bodyEnd = headerSize + len
    const expected = buf[bodyEnd] | (buf[bodyEnd + 1] << 8)
    const actual = this.codec.crc16(toU8(buf.slice(0, bodyEnd)))
    if (expected !== actual) {
      this._stats.crcErrors++
      return null
    }

    this._stats.frames++
    return {
      type: buf[0],
      seq: buf[1],
      payload: toU8(buf.slice(headerSize, bodyEnd))
    }
  }
}

/** 编码一帧（用于测试夹具与发送端），会按 codec 执行转义。 */
export function encodeFrame(
  frame: ParsedFrame,
  codec: Partial<FrameCodec> = {}
): Uint8Array {
  const c = { ...DEFAULT_FRAME_CODEC, ...codec }
  const { type, seq, payload } = frame
  const body: number[] = [type, seq, payload.length & 0xff, (payload.length >> 8) & 0xff]
  for (let i = 0; i < payload.length; i++) body.push(payload[i])

  const crc = c.crc16(toU8(body))
  body.push(crc & 0xff, (crc >> 8) & 0xff)

  const out: number[] = [c.sof]
  for (const byte of body) {
    if (byte === c.sof || byte === c.escape) {
      out.push(c.escape, byte ^ c.escapeXor)
    } else {
      out.push(byte)
    }
  }
  out.push(c.sof)
  return toU8(out)
}

/** CRC16-CCITT (FALSE)：poly=0x1021, init=0xFFFF。 */
export function crc16ccitt(data: Uint8Array): number {
  let crc = 0xffff
  for (let i = 0; i < data.length; i++) {
    crc ^= data[i] << 8
    for (let bit = 0; bit < 8; bit++) {
      crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff
    }
  }
  return crc
}

function toU8(arr: number[]): Uint8Array {
  return Uint8Array.from(arr)
}
