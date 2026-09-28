/**
 * 遥测传输抽象
 *
 * 具体实现按真实协议确定（USB CDC-ACM / HID / Vendor Bulk / Wi-Fi 等）。
 * 此处仅定义接口，并提供用于单测与诊断回放的 ReplayTransport。
 * 参见 TODOList.md §7.1。
 */

export type DataListener = (chunk: Uint8Array) => void
export type ErrorListener = (err: Error) => void

export interface TelemetryTransport {
  /** 建立链路，可重复调用（幂等） */
  open(): Promise<void>
  /** 关闭链路并释放 I/O 线程，必须幂等 */
  close(): Promise<void>
  /** 发送原始字节（控制/握手用，具体协议见 protocol.md） */
  send(data: Uint8Array): Promise<void>
  onData(listener: DataListener): () => void
  onError(listener: ErrorListener): () => void
}

/** 状态：把录制好的字节流按预定义分块回放，用于回放测试与诊断。 */
export class ReplayTransport implements TelemetryTransport {
  private chunks: Uint8Array[]
  private opened = false
  private dataListeners = new Set<DataListener>()

  constructor(chunks: Uint8Array[]) {
    this.chunks = chunks
  }

  async open(): Promise<void> {
    this.opened = true
  }

  async close(): Promise<void> {
    this.opened = false
    this.dataListeners.clear()
  }

  async send(): Promise<void> {
    // 回放传输不接受发送
  }

  onData(listener: DataListener): () => void {
    this.dataListeners.add(listener)
    return () => this.dataListeners.delete(listener)
  }

  onError(): () => void {
    return () => {}
  }

  /** 推入下一块数据；链路未打开时忽略。 */
  push(chunk: Uint8Array): void {
    if (!this.opened) return
    this.dataListeners.forEach((fn) => fn(chunk))
  }

  /** 一次性回放全部块。 */
  replayAll(): void {
    this.chunks.forEach((c) => this.push(c))
  }
}

/**
 * 把一段完整字节流按随机（可注入种子）大小分块，
 * 用于验证分帧器对粘包/拆包的健壮性。
 */
export function chunkBytes(
  data: Uint8Array,
  sizes: number[]
): Uint8Array[] {
  const out: Uint8Array[] = []
  let i = 0
  let k = 0
  while (i < data.length) {
    const size = sizes[k % sizes.length]
    out.push(data.slice(i, i + size))
    i += size
    k++
  }
  return out
}
