<template>
  <view class="top-bar">
    <view class="top-left">
      <view class="drone-name">{{ droneName }}</view>
      <view class="flight-status" :class="statusClass">{{ statusText }}</view>
    </view>

    <view class="top-center">
      <view class="signal-group">
        <view class="signal-item">
          <text class="signal-icon">📡</text>
          <text class="signal-value">{{ signalStrength }}</text>
        </view>
        <view class="signal-item">
          <text class="signal-icon">🛰</text>
          <text class="signal-value">{{ satellites }}</text>
        </view>
        <view class="signal-item">
          <text class="signal-icon">📶</text>
          <text class="signal-value">{{ linkQuality }}</text>
        </view>
      </view>
    </view>

    <view class="top-right">
      <view class="flight-time-label">飞行时间</view>
      <view class="flight-time-value">{{ flightTime }}</view>
    </view>
  </view>
</template>

<script>
const STATUS_MAP = {
  flying: { text: '飞行中', cls: 'status-flying' },
  landed: { text: '已降落', cls: 'status-landed' },
  rth: { text: '返航中', cls: 'status-rth' },
  disconnected: { text: '已断开', cls: 'status-disconnected' }
}

export default {
  name: 'TopBar',
  props: {
    droneName: { type: String, default: '无人机' },
    flightStatus: { type: String, default: 'disconnected' },
    flightTime: { type: String, default: '00:00:00' },
    signalStrength: { type: [String, Number], default: '--' },
    satellites: { type: [String, Number], default: '--' },
    linkQuality: { type: String, default: '--' }
  },
  computed: {
    statusText() { return (STATUS_MAP[this.flightStatus] || STATUS_MAP.disconnected).text },
    statusClass() { return (STATUS_MAP[this.flightStatus] || STATUS_MAP.disconnected).cls }
  }
}
</script>

<style scoped>
.top-bar {
  height: 80rpx;
  background: #1a1a2e;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 24rpx;
  border-bottom: 2rpx solid #2a2a4a;
  flex-shrink: 0;
}

.top-left { display: flex; align-items: center; gap: 16rpx; }

.drone-name { font-size: 26rpx; color: #e0e0e0; font-weight: bold; }

.flight-status {
  font-size: 20rpx;
  padding: 4rpx 16rpx;
  border-radius: 20rpx;
  color: #fff;
}

.status-flying { background: #23d160; }
.status-landed { background: #888; }
.status-rth { background: #ffdd57; color: #1a1a2e; }
.status-disconnected { background: #ff3860; }

.top-center { flex: 1; display: flex; justify-content: center; }
.signal-group { display: flex; gap: 32rpx; }
.signal-item { display: flex; align-items: center; gap: 6rpx; }
.signal-icon { font-size: 22rpx; }
.signal-value { font-size: 22rpx; color: #00d4ff; font-weight: bold; }

.top-right { display: flex; flex-direction: column; align-items: flex-end; }
.flight-time-label { font-size: 18rpx; color: #888; }
.flight-time-value { font-size: 28rpx; color: #00d4ff; font-weight: bold; font-family: monospace; }
</style>
