/**
 * 遥测服务兼容入口
 *
 * 实现已迁移到：
 *   - 领域模型 / store：domain/telemetry/store.ts
 *   - 分帧解析：services/telemetry-parser.ts
 *   - 传输抽象：services/telemetry-transport.ts
 *   - 应用编排：services/telemetry-service.ts
 *   - 开发 Mock：services/dev-mock.ts
 *
 * 本文件仅保留旧接口，避免 UI 一次性大改。
 */

import {
  telemetryStore,
  useTelemetry,
  useTelemetryView,
  formatTelemetry
} from '../domain/telemetry/store'

export { telemetryStore, useTelemetry, useTelemetryView, formatTelemetry }

/** 推送一帧已校验的遥测领域对象（TelemetryFrame）。 */
export function pushTelemetry(frame) {
  telemetryStore.update(frame)
}

export function setDisconnected() {
  telemetryStore.markDisconnected()
}

export function resetTelemetry() {
  telemetryStore.reset()
}
