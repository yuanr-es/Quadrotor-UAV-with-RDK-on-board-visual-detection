/**
 * 诊断与日志（§9）
 *
 * - 环形日志：限制条数，避免无限增长；
 * - 计数：连接错误、解析错误、丢帧等由各链路上报；
 * - 导出：生成可直接粘贴的文本；经纬度等敏感信息脱敏。
 *
 * 注意：真实原生崩溃/ANR 采集若接入第三方平台，需先做隐私与数据出境评估。
 */

export type LogLevel = 'info' | 'warn' | 'error'

export interface DiagnosticEntry {
  t: number
  level: LogLevel
  tag: string
  message: string
}

export interface DiagnosticsCounters {
  [key: string]: number
}

export interface DiagnosticsOptions {
  maxEntries?: number
  /** 导出时对经纬度等敏感字段脱敏 */
  desensitize?: boolean
}

export function getEnvironmentInfo(): Record<string, string> {
  const info: Record<string, string> = {}
  try {
    const uniAny = (globalThis as any).uni
    if (uniAny?.getSystemInfoSync) {
      const s = uniAny.getSystemInfoSync()
      info.platform = String(s.platform ?? '')
      info.system = String(s.system ?? '')
      info.model = String(s.model ?? '')
      info.brand = String(s.brand ?? '')
      info.appVersion = String(s.appVersion ?? '')
      info.SDKVersion = String(s.SDKVersion ?? '')
      return info
    }
  } catch {
    // 忽略，回退到浏览器
  }
  if (typeof navigator !== 'undefined') {
    info.platform = 'web'
    info.system = navigator.userAgent
  }
  return info
}

export class DiagnosticsLog {
  private maxEntries: number
  private desensitize: boolean
  private entries: DiagnosticEntry[] = []
  private counters: DiagnosticsCounters = {}

  constructor(opts: DiagnosticsOptions = {}) {
    this.maxEntries = opts.maxEntries ?? 500
    this.desensitize = opts.desensitize ?? true
  }

  add(level: LogLevel, tag: string, message: string): void {
    this.entries.push({ t: Date.now(), level, tag, message })
    if (this.entries.length > this.maxEntries) {
      this.entries.splice(0, this.entries.length - this.maxEntries)
    }
  }

  info(tag: string, message: string): void { this.add('info', tag, message) }
  warn(tag: string, message: string): void { this.add('warn', tag, message) }
  error(tag: string, message: string): void { this.add('error', tag, message) }

  /** 累加一个计数器（如 crcErrors、droppedFrames）。 */
  count(key: string, delta = 1): void {
    this.counters[key] = (this.counters[key] ?? 0) + delta
  }

  setCounters(counters: DiagnosticsCounters): void {
    this.counters = { ...this.counters, ...counters }
  }

  getCounters(): Readonly<DiagnosticsCounters> {
    return this.counters
  }

  getEntries(): readonly DiagnosticEntry[] {
    return this.entries
  }

  clear(): void {
    this.entries = []
    this.counters = {}
  }

  /** 导出为文本报告，默认脱敏经纬度。 */
  exportText(): string {
    const lines: string[] = []
    lines.push('# 诊断报告')
    lines.push(`导出时间: ${new Date().toISOString()}`)
    const env = getEnvironmentInfo()
    for (const [k, v] of Object.entries(env)) lines.push(`${k}: ${this.mask(v)}`)
    lines.push('')
    lines.push('## 计数器')
    for (const [k, v] of Object.entries(this.counters)) lines.push(`${k}: ${v}`)
    lines.push('')
    lines.push('## 日志')
    for (const e of this.entries) {
      const time = new Date(e.t).toISOString()
      lines.push(`[${time}] ${e.level.toUpperCase()} ${e.tag}: ${this.mask(e.message)}`)
    }
    return lines.join('\n')
  }

  private mask(text: string): string {
    if (!this.desensitize) return text
    // 把形如 30.2741 / -120.1551 的经纬度数值脱敏
    return text.replace(/-?\d{1,3}\.\d{4,}/g, '**.**')
  }
}

// ==================== 应用单例 ====================

export const diagnostics = new DiagnosticsLog({ maxEntries: 500, desensitize: true })
