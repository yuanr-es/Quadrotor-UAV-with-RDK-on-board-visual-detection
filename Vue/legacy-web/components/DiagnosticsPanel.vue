<template>
  <view class="diag-overlay" @tap.self="close">
    <view class="diag-panel">
      <view class="diag-header">
        <text class="diag-title">诊断</text>
        <text class="diag-close" @tap="close">✕</text>
      </view>

      <view class="diag-section">
        <view class="diag-section-title">环境</view>
        <view v-for="(v, k) in env" :key="k" class="diag-row">
          <text class="diag-key">{{ k }}</text>
          <text class="diag-val">{{ v }}</text>
        </view>
      </view>

      <view class="diag-section">
        <view class="diag-section-title">计数器</view>
        <view v-if="counterRows.length === 0" class="diag-empty">暂无</view>
        <view v-for="c in counterRows" :key="c.key" class="diag-row">
          <text class="diag-key">{{ c.key }}</text>
          <text class="diag-val">{{ c.value }}</text>
        </view>
      </view>

      <view class="diag-section diag-log-section">
        <view class="diag-section-title">日志（最近 {{ entries.length }} 条，已脱敏）</view>
        <scroll-view scroll-y class="diag-log-scroll">
          <view v-for="(e, i) in entries" :key="i" class="diag-log-line" :class="'lv-' + e.level">
            <text>{{ formatEntry(e) }}</text>
          </view>
        </scroll-view>
      </view>

      <view class="diag-actions">
        <view class="diag-btn" @tap="exportLog">导出 / 复制</view>
        <view class="diag-btn diag-btn-danger" @tap="clearLog">清空</view>
      </view>
    </view>
  </view>
</template>

<script>
import { diagnostics, getEnvironmentInfo } from '@/services/diagnostics'

export default {
  name: 'DiagnosticsPanel',
  emits: ['close'],
  data() {
    return {
      env: getEnvironmentInfo(),
      tick: 0
    }
  },
  computed: {
    entries() {
      // 依赖 tick 以便清空/更新后刷新
      void this.tick
      return diagnostics.getEntries().slice(-100).reverse()
    },
    counterRows() {
      void this.tick
      return Object.entries(diagnostics.getCounters()).map(([key, value]) => ({ key, value }))
    }
  },
  methods: {
    formatEntry(e) {
      const d = new Date(e.t)
      const hms = `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}:${String(d.getSeconds()).padStart(2, '0')}`
      return `${hms} ${e.tag}: ${e.message}`
    },
    exportLog() {
      const text = diagnostics.exportText()
      const uniAny = globalThis.uni
      if (uniAny?.setClipboardData) {
        uniAny.setClipboardData({ data: text })
      } else if (typeof navigator !== 'undefined' && navigator.clipboard) {
        navigator.clipboard.writeText(text)
      }
      console.log(text)
    },
    clearLog() {
      diagnostics.clear()
      this.tick++
    },
    close() {
      this.$emit('close')
    }
  }
}
</script>

<style scoped>
.diag-overlay {
  position: fixed;
  inset: 0;
  background: rgba(0, 0, 0, 0.6);
  z-index: 999;
  display: flex;
  align-items: center;
  justify-content: center;
}

.diag-panel {
  width: 640rpx;
  max-height: 80vh;
  background: #141428;
  border: 2rpx solid #2a2a4a;
  border-radius: 12rpx;
  padding: 16rpx;
  display: flex;
  flex-direction: column;
  gap: 12rpx;
}

.diag-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}
.diag-title { font-size: 26rpx; color: #00d4ff; font-weight: bold; }
.diag-close { font-size: 26rpx; color: #888; }

.diag-section { display: flex; flex-direction: column; gap: 4rpx; }
.diag-section-title {
  font-size: 20rpx;
  color: #888;
  border-bottom: 1rpx solid #2a2a4a;
  padding-bottom: 4rpx;
  margin-bottom: 4rpx;
}

.diag-row { display: flex; justify-content: space-between; font-size: 18rpx; }
.diag-key { color: #888; }
.diag-val { color: #e0e0e0; }

.diag-log-section { flex: 1; min-height: 200rpx; }
.diag-log-scroll { max-height: 360rpx; }
.diag-log-line { font-size: 16rpx; color: #aaa; font-family: monospace; }
.lv-warn { color: #ffdd57; }
.lv-error { color: #ff3860; }
.diag-empty { font-size: 18rpx; color: #555; }

.diag-actions { display: flex; gap: 12rpx; }
.diag-btn {
  flex: 1;
  text-align: center;
  padding: 12rpx;
  border-radius: 8rpx;
  font-size: 22rpx;
  background: #00d4ff;
  color: #0f0f1a;
  font-weight: bold;
}
.diag-btn-danger { background: #ff3860; color: #fff; }
</style>
